#include "labbridge/stream.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
namespace labbridge {
Json parse_stream_message(const std::string &text) {
  unsigned depth = 0;
  bool quoted = false, escaped = false;
  for (char byte : text) {
    if (quoted) {
      if (escaped)
        escaped = false;
      else if (byte == '\\')
        escaped = true;
      else if (byte == '"')
        quoted = false;
    } else if (byte == '"')
      quoted = true;
    else if (byte == '{' || byte == '[') {
      if (++depth > 16)
        throw std::invalid_argument("Stream JSON nesting exceeds 16");
    } else if ((byte == '}' || byte == ']') && depth)
      --depth;
  }
  return Json::parse(text);
}
bool write_stream_bytes(const char *data, size_t size, const StreamSend &send) {
  while (size) {
    auto count = send(data, size);
    if (count <= 0 || size_t(count) > size)
      return false;
    data += count;
    size -= count;
  }
  return true;
}
namespace {
void hash_byte(uint64_t &hash, unsigned char byte) {
  hash = (hash ^ byte) * 1099511628211ULL;
}
void hash_text(uint64_t &hash, const char *text) {
  if (text)
    for (; *text; ++text)
      hash_byte(hash, static_cast<unsigned char>(*text));
  hash_byte(hash, 0);
}
void hash_json(uint64_t &hash, const cJSON *value, bool root = false) {
  hash_byte(hash, value->type & 255);
  if (cJSON_IsNumber(value)) {
    const auto *bytes =
        reinterpret_cast<const unsigned char *>(&value->valuedouble);
    for (size_t n = 0; n < sizeof(value->valuedouble); ++n)
      hash_byte(hash, bytes[n]);
  } else if (cJSON_IsString(value) || cJSON_IsRaw(value)) {
    hash_text(hash, value->valuestring);
  } else {
    for (auto child = value->child; child; child = child->next) {
      if (root && child->string &&
          std::strcmp(child->string, "measurements") == 0)
        continue;
      hash_text(hash, child->string);
      hash_json(hash, child);
    }
  }
  hash_byte(hash, 255);
}
} // namespace
uint64_t StreamSession::status_fingerprint(const Json &body) {
  uint64_t hash = 14695981039346656037ULL;
  hash_json(hash, body.get(), true);
  return hash;
}
Json StreamSession::reply(const Json &request, Response r) const {
  Json result, headers;
  if (request.has("id"))
    result.set("id", request.item("id"));
  for (const auto &h : r.headers)
    headers.set(h.first, h.second);
  result.set("status", double(r.status))
      .set("body", std::move(r.body))
      .set("headers", std::move(headers));
  return result;
}
Json StreamSession::receive(const Json &r, uint64_t now) {
  if (overloaded_)
    return reply(r, problem(503, "Stream operation budget exceeded; inspect "
                                 "accepted operations over HTTP"));
  if (!authenticated()) {
    token_.clear();
    auto credential = r.text("token", r.text("csrf_token"));
    if (!auth_failed_ && now <= 3000 && r.text("type") == "auth" &&
        r.has("token") != r.has("csrf_token") && verify_(credential) &&
        !credential.empty()) {
      token_ = credential;
      return Json().set("type", "authenticated").set("boot_id", boot_);
    }
    auth_failed_ = true;
    return reply(
        r,
        problem(401, "Authenticate as the first message within three seconds"));
  }
  if (r.text("type") == "subscribe") {
    auto device = r.text("device_id");
    auto interval = r.number("interval_ms", 1000);
    if (device.empty() || device.size() > 48 ||
        !std::all_of(device.begin(), device.end(),
                     [](unsigned char c) {
                       return std::isalnum(c) || c == '_' || c == '-';
                     }) ||
        (interval != 500 && interval != 1000 && interval != 2000))
      return reply(
          r, problem(400, "Select a device and interval 500, 1000 or 2000 ms"));
    auto selected = dispatch_("GET", "/api/v1/devices/" + device, Json(),
                              {{"Authorization", "Bearer " + token_}});
    if (selected.status != 200)
      return reply(r, selected);
    device_ = device;
    interval_ = interval;
    pending_ = false;
    gap_ = false;
    return reply(r, Response{200, Json().set("subscribed", device_)});
  }
  auto method = r.text("method"), path = r.text("path");
  if (path == "/management/token")
    return reply(r, problem(403, "Token retrieval is USB-only"));
  if (path.rfind("/api/v1/", 0) != 0 ||
      path.find_first_of("?#\r\n") != std::string::npos || path.size() > 256 ||
      (method != "GET" && method != "PUT" && method != "POST" &&
       method != "DELETE"))
    return reply(r, problem(400, "Expected a local API method and path"));
  Headers headers{{"Authorization", "Bearer " + token_}};
  auto h = r.item("headers");
  for (auto name : {"X-Lab-Lease", "Idempotency-Key", "If-Match"})
    if (h.has(name))
      headers[name] = h.text(name);
  auto response = dispatch_(method, path, r.item("body"), headers);
  auto operation = response.body.text("id");
  if (response.status == 202 && !operation.empty() &&
      std::find(operations_.begin(), operations_.end(), operation) ==
          operations_.end()) {
    if (operations_.size() == 16)
      overloaded_ = true; // Reply with this accepted ID, then close; never
                          // forget a pending result.
    else
      operations_.push_back(operation);
  }
  return reply(r, std::move(response));
}
void StreamSession::offer_snapshot(Json body) {
  if (!authenticated() || overloaded_)
    return;
  bool gap = gap_ || pending_;
  // Release the previous encoded slot before constructing its replacement.
  // Keeping both payloads alive doubles the peak for a slow consumer.
  std::string().swap(snapshot_);
  pending_ = false;
  completed_.clear();
  if (auto operations = body.at("operations"))
    for (auto entry = operations->child; entry; entry = entry->next) {
      auto status = cJSON_GetObjectItemCaseSensitive(entry, "status");
      auto id = cJSON_GetObjectItemCaseSensitive(entry, "id");
      if (cJSON_IsString(status) && cJSON_IsString(id) &&
          std::strcmp(status->valuestring, "running") != 0 &&
          std::strcmp(status->valuestring, "pending") != 0)
        completed_.push_back(id->valuestring);
    }
  Json event;
  event.set("type", "snapshot")
      .set("boot_id", boot_)
      .set("sequence", double(++sequence_))
      .set("gap", gap)
      .set("body", std::move(body));
  auto encoded = event.dump();
  if (encoded.size() > 32768)
    throw std::length_error("Stream snapshot exceeds output budget");
  snapshot_ = std::move(encoded);
  pending_ = true;
  gap_ = gap;
}
void StreamSession::prepare_request(const Json &request) {
  auto path = request.text("path");
  constexpr auto suffix = "/heartbeat";
  if (request.text("method") == "POST" && path.size() >= std::strlen(suffix) &&
      path.compare(path.size() - std::strlen(suffix), std::strlen(suffix),
                   suffix) == 0)
    return; // Small, periodic heartbeat replies must not create telemetry gaps.
  discard_snapshot();
}
void StreamSession::discard_snapshot() {
  if (pending_) {
    pending_ = false;
    gap_ = true;
    std::string().swap(snapshot_);
  }
}
std::string StreamSession::take_encoded_snapshot() {
  if (!authenticated() || !pending_)
    return {};
  pending_ = false;
  gap_ = false;
  for (const auto &id : completed_)
    operations_.erase(std::remove(operations_.begin(), operations_.end(), id),
                      operations_.end());
  completed_.clear();
  return std::move(snapshot_);
}
Json StreamSession::take_snapshot() {
  auto encoded = take_encoded_snapshot();
  return encoded.empty() ? Json() : Json::parse(encoded);
}
} // namespace labbridge
