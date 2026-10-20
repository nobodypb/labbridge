#include "labbridge/auth.hpp"
#include <algorithm>

namespace labbridge {
namespace {
std::string header(const Headers &h, const std::string &name) {
  for (const auto &p : h) {
    auto key = p.first;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (key == name)
      return p.second;
  }
  return {};
}
bool equal(const std::string &a, const std::string &b) {
  size_t difference = a.size() ^ b.size();
  for (size_t i = 0; i < std::max(a.size(), b.size()); ++i)
    difference |= (i < a.size() ? static_cast<unsigned char>(a[i]) : 0) ^
                  (i < b.size() ? static_cast<unsigned char>(b[i]) : 0);
  return difference == 0;
}
bool same_origin(const Headers &h, const AuthContext &c) {
  return !c.origin.empty() && header(h, "origin") == c.origin;
}
bool json_content(const Headers &h) {
  auto value = header(h, "content-type");
  value = value.substr(0, value.find(';'));
  return value == "application/json";
}
bool configured(const Json &record) {
  return !cJSON_IsObject(record.get()) || record.get()->child != nullptr;
}
bool hex(const std::string &s) {
  return s.size() == 64 &&
         s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
bool usable(const Json &record) {
  return record.number("version") == Auth::verifier_version &&
         record.number("iterations") == Auth::iterations &&
         hex(record.text("salt")) && hex(record.text("hash"));
}
Response limited(unsigned seconds, const std::string &detail) {
  auto result = problem(429, detail);
  result.headers["Retry-After"] = std::to_string(seconds);
  return result;
}
const char *clear_cookie =
    "LabBridgeSession=; Path=/; Max-Age=0; HttpOnly; SameSite=Strict";
} // namespace
Auth::Auth(std::string bearer, Json record, Random random, Derive derive,
           Persist persist, Clock clock)
    : bearer_(std::move(bearer)), record_(std::move(record)),
      random_(std::move(random)), derive_(std::move(derive)),
      persist_(std::move(persist)), clock_(std::move(clock)) {}

void Auth::prune(uint64_t now) {
  for (auto it = sessions_.begin(); it != sessions_.end();)
    if (it->second.deadline <= now)
      it = sessions_.erase(it);
    else
      ++it;
  while (!failures_.empty() && now - failures_.front() >= 60000)
    failures_.pop_front();
}
std::string Auth::session_id(const Headers &h, const AuthContext &c) const {
  if (!c.local && c.origin.empty())
    return {};
  auto cookies = header(h, "cookie");
  size_t start = 0;
  std::string found;
  while (start < cookies.size()) {
    auto end = cookies.find(';', start);
    auto cookie =
        cookies.substr(start, end == std::string::npos ? end : end - start);
    auto first = cookie.find_first_not_of(" \t");
    if (first != std::string::npos)
      cookie.erase(0, first);
    if (cookie.rfind("LabBridgeSession=", 0) == 0) {
      if (!found.empty())
        return {}; // Duplicate cookie is ambiguous.
      found = cookie.substr(17);
    }
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  auto it = sessions_.find(found);
  return it != sessions_.end() && it->second.origin == c.origin ? found : "";
}
Json Auth::status(const Headers &h, const AuthContext &c, uint64_t now) const {
  Json result;
  auto id = session_id(h, c);
  result.set("password_configured", configured(record_))
      .set("setup_allowed",
           !configured(record_) &&
               (c.local || (c.access_point && !c.origin.empty())))
      .set("authenticated", !id.empty());
  if (!id.empty()) {
    const auto &s = sessions_.at(id);
    result.set("csrf_token", s.csrf)
        .set("expires_in_ms", double(s.deadline - now));
  }
  return result;
}
Response Auth::authorize_locked(const std::string &method,
                                const std::string &path, const Headers &h,
                                const AuthContext &c) const {
  if (c.local)
    return {200, Json()};
  if (path == "/management/token")
    return problem(403, "Token retrieval is USB-only");
  if ((method == "GET" && path == "/api/v1/auth/status") ||
      (method == "POST" &&
       (path == "/api/v1/auth/setup" || path == "/api/v1/auth/login")))
    return {200, Json()};
  bool session_only = route(path);
  if (!session_only && !bearer_.empty() &&
      equal(header(h, "authorization"), "Bearer " + bearer_))
    return {200, Json()};
  auto id = session_id(h, c);
  if (!id.empty()) {
    if (method != "GET" &&
        (!same_origin(h, c) ||
         !equal(header(h, "x-lab-csrf"), sessions_.at(id).csrf) ||
         ((method == "POST" || method == "PUT") && !json_content(h))))
      return problem(403, "Same-origin JSON and session CSRF header required");
    return {200, Json()};
  }
  if (!session_only && c.access_point &&
      ((path == "/api/v1/wifi" && (method == "GET" || method == "PUT")) ||
       (path == "/api/v1/wifi/scan" && method == "GET")))
    return {200, Json()};
  return problem(
      401, "Sign in with the device password or supply an API bearer token");
}
Response Auth::authorize(const std::string &method, const std::string &path,
                         const Headers &h, const AuthContext &c) {
  std::lock_guard<std::mutex> lock(mutex_);
  prune(clock_());
  return authorize_locked(method, path, h, c);
}
Response Auth::request(const std::string &method, const std::string &path,
                       const Json &body, const Headers &h,
                       const AuthContext &c) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto now = clock_();
  prune(now);
  const std::string root = "/api/v1/auth/";
  if (method == "GET" && path == root + "status")
    return {200, status(h, c, now)};
  bool setup = method == "POST" && path == root + "setup";
  bool login = method == "POST" && path == root + "login";
  bool change = method == "PUT" && path == root + "password";
  bool logout = method == "DELETE" && path == root + "session";
  if (!setup && !login && !change && !logout)
    return problem(404, "Unknown authentication route");
  if (logout || change) {
    auto access = authorize_locked(method, path, h, c);
    if (access.status != 200)
      return access;
  } else if (!c.local && (!same_origin(h, c) || !json_content(h))) {
    return problem(403, "Same-origin JSON required");
  }
  if (logout) {
    sessions_.erase(session_id(h, c));
    return {200, status({}, c, now), {{"Set-Cookie", clear_cookie}}};
  }
  std::string reason;
  if (!validate_request(setup   ? "setupUiPassword"
                        : login ? "loginUi"
                                : "changeUiPassword",
                        body, reason))
    return problem(400, reason);
  if (setup && !(c.local || c.access_point))
    return problem(403,
                   "Choose the first password through the setup AP or USB");
  if (setup && configured(record_))
    return problem(409, "UI password already configured");
  if (!login && body.text("password") != body.text("confirm_password"))
    return problem(422, "Password entries must match");
  if (change && !c.local && !body.has("current_password"))
    return problem(400, "Current password required");
  if ((login || (change && !c.local)) && !usable(record_))
    return problem(503, "UI password unavailable; set or recover it over USB");
  if (busy_)
    return limited(1, "Password verification busy; try again later");
  if (!c.local && failures_.size() >= 5)
    return limited(unsigned((60000 - (now - failures_.front()) + 999) / 1000),
                   "Too many incorrect passwords; wait before trying again");
  if (login && sessions_.size() >= 8)
    return limited(60, "Eight browser sessions are active; sign out in another "
                       "browser or wait for a session to expire");
  auto record = record_;
  auto original_id = session_id(h, c);
  bool verified = true;
  Json replacement;
  std::string new_id, csrf;
  busy_ = true;
  lock.unlock();
  try {
    if (login || (change && !c.local))
      verified =
          equal(derive_(body.text(login ? "password" : "current_password"),
                        record.text("salt"), iterations),
                record.text("hash"));
    if (verified && !login) {
      auto salt = random_();
      auto hash = derive_(body.text("password"), salt, iterations);
      if (!hex(salt) || !hex(hash))
        throw std::runtime_error("Invalid crypto result");
      replacement.set("version", double(verifier_version))
          .set("iterations", double(iterations))
          .set("salt", salt)
          .set("hash", hash);
    }
    if (verified && login) {
      new_id = random_();
      csrf = random_();
      if (!hex(new_id) || !hex(csrf))
        throw std::runtime_error("Invalid random result");
    }
  } catch (...) {
    lock.lock();
    busy_ = false;
    return problem(503, "Password verification unavailable");
  }
  lock.lock();
  busy_ = false;
  now = clock_();
  prune(now);
  if (!verified) {
    failures_.push_back(now);
    return problem(401, "Incorrect password");
  }
  // A session can expire or be explicitly revoked while its hash is computed.
  if (change && !c.local &&
      (original_id.empty() || session_id(h, c) != original_id))
    return problem(401, "Session ended; sign in again");
  if (login) {
    if (sessions_.count(new_id))
      return problem(503, "Session creation unavailable");
    auto previous = session_id(h, c);
    if (!previous.empty())
      sessions_.erase(previous);
    sessions_[new_id] = {csrf, c.origin, now + session_ttl_ms};
    auto authenticated = h;
    authenticated["Cookie"] = "LabBridgeSession=" + new_id;
    return {200,
            status(authenticated, c, now),
            {{"Set-Cookie",
              "LabBridgeSession=" + new_id +
                  "; Path=/; Max-Age=43200; HttpOnly; SameSite=Strict"}}};
  }
  try {
    persist_(replacement);
  } catch (...) {
    return problem(503, "Password could not be saved");
  }
  record_ = replacement;
  sessions_.clear();
  failures_.clear();
  return {200, status({}, c, now),
          setup ? Headers{} : Headers{{"Set-Cookie", clear_cookie}}};
}
} // namespace labbridge
