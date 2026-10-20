#include "labbridge/auth.hpp"
#include <cassert>
#include <condition_variable>
#include <iomanip>
#include <sstream>
#include <thread>
using namespace labbridge;
struct Actor : Driver {
  unsigned stops = 0;
  Json describe(uint64_t) override {
    return Json::parse(R"({"state":"ready","output":"off"})");
  }
  Json measurements(uint64_t) override { return Json(); }
  bool supports(const std::string &) const override { return true; }
  void tick(uint64_t) override {}
  void stop(uint64_t) override { ++stops; }
  Json execute(const std::string &, const Json &, uint64_t) override {
    return Json();
  }
};
int main() {
  uint64_t now = 100;
  unsigned random = 0, hashes = 0;
  Json saved;
  auto entropy = [&] {
    return std::string(60, 'a') + std::to_string(1000 + random++);
  };
  auto derive = [&](const std::string &password, const std::string &salt,
                    unsigned rounds) {
    assert(rounds == 6000);
    ++hashes;
    std::ostringstream hash;
    for (int i = 0; i < 4; ++i)
      hash << std::hex << std::setw(16) << std::setfill('0')
           << std::hash<std::string>{}(password + salt + std::to_string(i));
    return hash.str(); // Test double, never used by firmware.
  };
  auto persist = [&](const Json &record) { saved = record; };
  Auth auth("synthetic-bearer", Json(), entropy, derive, persist,
            [&] { return now; });
  AuthContext lan{false, false, "http://node.local"};
  AuthContext ap{false, true, "http://192.168.4.1"};
  AuthContext usb{true, false, ""};
  Headers post{{"Origin", lan.origin}, {"Content-Type", "application/json"}};
  Headers ap_post{{"Origin", ap.origin}, {"Content-Type", "application/json"}};
  const std::string scan = "/api/v1/wifi/scan", root = "/api/v1/auth/";
  auto body = Json::parse(
      R"({"password":" spaces preserved ","confirm_password":" spaces preserved "})");
  assert(auth.authorize("GET", scan, {}, lan).status == 401);
  assert(auth.authorize("GET", scan, {}, ap).status == 200);
  assert(auth.authorize("PUT", "/api/v1/wifi", {}, ap).status == 200);
  assert(auth.authorize("PUT", "/api/v1/wifi/ap", {}, ap).status == 401);
  assert(auth.authorize("GET", "/api/v1/devices", {}, ap).status == 401);
  assert(auth.authorize("GET", scan,
                        {{"Authorization", "Bearer synthetic-bearer"}}, lan)
             .status == 200);
  assert(auth.authorize("GET", scan, {}, usb).status == 200);
  assert(auth.request("GET", root + "status", Json(), {}, lan)
             .body.boolean("setup_allowed") == false);
  assert(auth.request("GET", root + "status", Json(), {}, ap)
             .body.boolean("setup_allowed"));
  assert(!auth.request("GET", root + "status", Json(), {}, {false, true, ""})
              .body.boolean("setup_allowed"));
  assert(auth.request("POST", root + "setup", body, post, lan).status == 403);
  assert(auth.request("POST", root + "setup", body, {}, ap).status == 403);
  auto mismatch = body;
  mismatch.set("confirm_password", "something different");
  assert(auth.request("POST", root + "setup", mismatch, ap_post, ap).status ==
         422);
  auto unknown = body;
  unknown.set("unrecognized", true);
  assert(auth.request("POST", root + "setup", unknown, ap_post, ap).status ==
         400);
  auto setup = auth.request("POST", root + "setup", body, ap_post, ap);
  assert(setup.status == 200 && saved.has("hash") && !saved.has("password"));
  assert(!setup.headers.count("Set-Cookie"));
  assert(auth.request("POST", root + "setup", body, ap_post, ap).status == 409);
  auto password = Json::parse(R"({"password":" spaces preserved "})");
  assert(auth.request("POST", root + "login", password,
                      {{"Origin", "http://evil.example"},
                       {"Content-Type", "application/json"}},
                      lan)
             .status == 403);
  auto login = auth.request("POST", root + "login", password, post, lan);
  assert(login.status == 200);
  auto cookie = login.headers.at("Set-Cookie");
  assert(cookie.find("HttpOnly") != std::string::npos &&
         cookie.find("SameSite=Strict") != std::string::npos);
  auto id = cookie.substr(0, cookie.find(';'));
  Headers session{{"Cookie", id},
                  {"Origin", lan.origin},
                  {"Content-Type", "application/json"}};
  assert(auth.authorize("GET", scan, session, lan).status == 200);
  assert(auth.authorize("GET", scan, session, {false, false, ""}).status ==
         401);
  assert(auth.authorize("POST", "/api/v1/leases", session, lan).status == 403);
  session["X-Lab-CSRF"] = login.body.text("csrf_token");
  assert(auth.authorize("POST", "/api/v1/leases", session, lan).status == 200);
  auto foreign = session;
  foreign["Origin"] = "http://evil.example";
  assert(auth.authorize("POST", "/api/v1/leases", foreign, lan).status == 403);
  assert(auth.authorize("GET", "/management/token", session, lan).status ==
         403);
  assert(auth.request("GET", root + "status", Json(), session, lan)
             .body.boolean("authenticated"));
  // Reload reconstructs status from the cookie, rather than localStorage.
  assert(auth.request("GET", root + "status", Json(), session, lan)
             .body.text("csrf_token") == session["X-Lab-CSRF"]);
  assert(
      auth.request("DELETE", root + "session", Json(), session, lan).status ==
      200);
  assert(auth.authorize("GET", scan, session, lan).status == 401);
  login = auth.request("POST", root + "login", password, post, lan);
  cookie = login.headers.at("Set-Cookie");
  session["Cookie"] = cookie.substr(0, cookie.find(';'));
  session["X-Lab-CSRF"] = login.body.text("csrf_token");
  now += Auth::session_ttl_ms;
  assert(auth.authorize("GET", scan, session, lan).status == 401);
  auto wrong = Json::parse(R"({"password":"wrong password"})");
  for (int i = 0; i < 5; ++i)
    assert(auth.request("POST", root + "login", wrong, post, lan).status ==
           401);
  auto attempts = hashes;
  assert(auth.request("POST", root + "login", password, post, lan).status ==
             429 &&
         hashes == attempts);
  assert(auth.authorize("POST", "/api/v1/devices/load/stop",
                        {{"Authorization", "Bearer synthetic-bearer"}}, lan)
             .status == 200);
  now += 60001;
  for (int i = 0; i < 8; ++i)
    assert(auth.request("POST", root + "login", password, post, lan).status ==
           200);
  assert(auth.request("POST", root + "login", password, post, lan).status ==
         429);
  // USB replacement invalidates all sessions and bypasses network lockout.
  assert(auth.request("PUT", root + "password", body, {}, usb).status == 200);
  login = auth.request("POST", root + "login", password, post, lan);
  cookie = login.headers.at("Set-Cookie");
  session["Cookie"] = cookie.substr(0, cookie.find(';'));
  session["X-Lab-CSRF"] = login.body.text("csrf_token");
  auto change = body;
  change.set("current_password", "wrong password");
  assert(auth.request("PUT", root + "password", change, session, lan).status ==
         401);
  change.set("current_password", " spaces preserved ");
  assert(auth.request("PUT", root + "password", change, session, lan).status ==
         200);
  assert(auth.authorize("GET", scan, session, lan).status == 401);
  Auth reboot("synthetic-bearer", saved, entropy, derive, persist,
              [&] { return now; });
  assert(reboot.authorize("GET", scan, session, lan).status == 401);
  assert(reboot.request("POST", root + "login", password, post, lan).status ==
         200);
  // Slow password verification must not hold the authorization mutex.
  std::mutex gate;
  std::condition_variable cv;
  bool entered = false, resume = false;
  Auth slow(
      "synthetic-bearer", saved, entropy,
      [&](const std::string &p, const std::string &s, unsigned r) {
        std::unique_lock<std::mutex> lock(gate);
        entered = true;
        cv.notify_one();
        cv.wait(lock, [&] { return resume; });
        return derive(p, s, r);
      },
      persist, [&] { return now; });
  std::thread worker([&] {
    assert(slow.request("POST", root + "login", password, post, lan).status ==
           200);
  });
  {
    std::unique_lock<std::mutex> lock(gate);
    cv.wait(lock, [&] { return entered; });
  }
  assert(slow.authorize("POST", "/api/v1/devices/load/stop",
                        {{"Authorization", "Bearer synthetic-bearer"}}, lan)
             .status == 200);
  Node node("synthetic", "boot", "test");
  auto actor = std::make_shared<Actor>();
  node.add("actor", actor);
  auto lease = node.request(
      "POST", "/api/v1/leases",
      Json::parse(R"({"device_ids":["actor"],"ttl_ms":1000})"), {}, now);
  assert(lease.status == 200);
  assert(node.request("POST",
                      "/api/v1/leases/" + lease.body.text("id") + "/heartbeat",
                      Json(), {}, now)
             .status == 200);
  assert(node.request("POST", "/api/v1/devices/actor/stop", Json(),
                      {{"Idempotency-Key", "during-hash"}}, now)
             .status == 202);
  assert(actor->stops == 1);
  assert(slow.request("POST", root + "login", password, post, lan).status ==
         429);
  {
    std::lock_guard<std::mutex> lock(gate);
    resume = true;
  }
  cv.notify_one();
  worker.join();
  std::string unicode;
  for (int i = 0; i < 128; ++i)
    unicode += "\xc3\xa4";
  Json unicode_password;
  unicode_password.set("password", unicode).set("confirm_password", unicode);
  assert(auth.request("PUT", root + "password", unicode_password, {}, usb)
             .status == 200);
  Auth usb_setup("synthetic-bearer", Json(), entropy, derive, persist,
                 [&] { return now; });
  assert(usb_setup.request("POST", root + "setup", body, {}, usb).status ==
         200);
  assert(usb_setup
             .request("POST", root + "login",
                      Json::parse(R"({"password":"spaces preserved"})"), post,
                      lan)
             .status == 401);
  Auth unavailable(
      "synthetic-bearer", saved, entropy, derive,
      [](const Json &) { throw std::runtime_error("storage unavailable"); },
      [&] { return now; });
  assert(
      unavailable.request("PUT", root + "password", unicode_password, {}, usb)
          .status == 503);
  assert(
      unavailable.request("POST", root + "login", password, post, lan).status ==
      200);
  // A pre-release high-cost verifier must never run a 115-second login or
  // be silently reinterpreted with fewer rounds. Trusted USB replaces it.
  auto legacy_record = saved;
  legacy_record.set("version", 1.).set("iterations", 600000.);
  Auth legacy("synthetic-bearer", legacy_record, entropy, derive, persist,
              [&] { return now; });
  auto before_legacy = hashes;
  assert(legacy.request("POST", root + "login", password, post, lan).status ==
         503);
  assert(hashes == before_legacy);
  assert(legacy
             .authorize("GET", scan,
                        {{"Authorization", "Bearer synthetic-bearer"}}, lan)
             .status == 200);
  assert(legacy.request("PUT", root + "password", body, {}, usb).status == 200);
  assert(saved.number("version") == 2 && saved.number("iterations") == 6000);
  assert(legacy.request("POST", root + "login", password, post, lan).status ==
         200);
  auto corrupt = Json::parse(R"({"version":999})");
  Auth recovery("synthetic-bearer", corrupt, entropy, derive, persist,
                [&] { return now; });
  assert(!recovery.request("GET", root + "status", Json(), {}, ap)
              .body.boolean("setup_allowed"));
  assert(recovery.request("POST", root + "login", password, post, lan).status ==
         503);
  assert(recovery.request("PUT", root + "password", body, {}, usb).status ==
         200);
}
