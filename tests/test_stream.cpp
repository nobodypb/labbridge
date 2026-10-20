#include "labbridge/stream.hpp"
#include <cassert>
using namespace labbridge;
int main() {
  auto quoted = parse_stream_message(R"({"type":"auth","token":"[{}] \\ \""})");
  assert(quoted.text("type") == "auth");
  std::string nested = "0";
  for (unsigned i = 0; i < 17; ++i)
    nested = "[" + nested + "]";
  bool depth_rejected = false;
  try {
    parse_stream_message(nested);
  } catch (const std::invalid_argument &) {
    depth_rejected = true;
  }
  assert(depth_rejected);
  std::string sent;
  auto payload = std::string("bounded stream frame");
  assert(write_stream_bytes(payload.data(), payload.size(),
                            [&](const char *data, size_t size) {
                              auto count = std::min(size, size_t(3));
                              sent.append(data, count);
                              return int(count);
                            }));
  assert(sent == payload);
  unsigned attempts = 0;
  assert(!write_stream_bytes(
      payload.data(), payload.size(),
      [&](const char *, size_t) { return ++attempts < 3 ? 2 : -1; }));
  assert(attempts == 3);
  assert(!write_stream_bytes(payload.data(), payload.size(),
                             [](const char *, size_t) { return 0; }));
  auto status = Json::parse(
      R"({"measurements":{"sequence":1},"actors":[{"output":"off"}],"details":{"settings":{"mode":0}}})");
  auto digest = StreamSession::status_fingerprint(status);
  status.set("measurements", Json::parse(R"({"sequence":2})"));
  assert(StreamSession::status_fingerprint(status) == digest);
  status.set("actors", Json::parse(R"([{"output":"on"}])"));
  assert(StreamSession::status_fingerprint(status) != digest);
  status.set("actors", Json::parse(R"([{"output":"off"}])"));
  status.set("details", Json::parse(R"({"settings":{"mode":1}})"));
  assert(StreamSession::status_fingerprint(status) != digest);
  Node node("n", "boot-a", "test");
  assert(node.stream_snapshot("", {}, 0).item("node").text("boot_id") ==
         "boot-a");
  unsigned calls = 0;
  auto dispatch = [&](const std::string &, const std::string &path,
                      const Json &, const Headers &h) {
    ++calls;
    assert(h.at("Authorization") == "Bearer test-secret");
    return path == "/management/token" ? problem(403, "USB only")
                                       : Response{200, Json().set("ok", true)};
  };
  StreamSession rejected(
      "boot-a", [](const std::string &s) { return s == "test-secret"; },
      dispatch);
  assert(
      rejected
          .receive(Json::parse(
                       R"({"id":"r1","method":"GET","path":"/api/v1/node"})"),
                   0)
          .number("status") == 401);
  assert(calls == 0);
  assert(rejected.receive(Json::parse(R"({"type":"auth","token":"wrong"})"), 1)
             .number("status") == 401);
  assert(
      rejected
          .receive(Json::parse(R"({"type":"auth","token":"test-secret"})"), 2)
          .number("status") == 401);
  StreamSession s(
      "boot-a", [](const std::string &token) { return token == "test-secret"; },
      dispatch);
  assert(s.receive(Json::parse(R"({"type":"auth","token":"test-secret"})"), 2)
             .text("type") == "authenticated");
  auto response = s.receive(
      Json::parse(
          R"({"id":"r2","method":"GET","path":"/api/v1/node","headers":{"Authorization":"Bearer attacker"}})"),
      3);
  assert(response.number("status") == 200 && response.text("id") == "r2" &&
         calls == 1);
  assert(
      s.receive(Json::parse(
                    R"({"id":"r3","method":"GET","path":"/management/token"})"),
                4)
          .number("status") == 403);
  assert(
      s.receive(
           Json::parse(
               R"({"id":"r4","method":"PUT","path":"/api/v1/devices/x/output","headers":{"X-Lab-Lease":"l","Idempotency-Key":"k"},"body":{"enabled":true}})"),
           5)
          .number("status") == 200);
  s.offer_snapshot(Json().set("sample", 1.));
  s.offer_snapshot(Json().set("sample", 2.));
  auto event = s.take_snapshot();
  assert(event.text("boot_id") == "boot-a" && event.boolean("gap") &&
         event.item("body").number("sample") == 2);
  assert(!s.has_snapshot());
  s.offer_snapshot(Json().set("sample", 3.));
  auto encoded = s.take_encoded_snapshot();
  assert(Json::parse(encoded).item("body").number("sample") == 3 &&
         !s.has_snapshot());

  s.offer_snapshot(Json().set("sample", 4.));
  s.prepare_request(Json::parse(
      R"({"method":"POST","path":"/api/v1/leases/example/heartbeat","body":{}})"));
  assert(s.has_snapshot());
  s.prepare_request(Json::parse(
      R"({"method":"POST","path":"/api/v1/devices/example/stop","body":{}})"));
  assert(!s.has_snapshot());
  s.mark_gap();
  s.offer_snapshot(Json().set("sample", 4.));
  assert(s.take_snapshot().boolean("gap"));

  auto subscribed = s.receive(
      Json::parse(
          R"({"id":"subscription","type":"subscribe","device_id":"x","interval_ms":500})"),
      7);
  assert(subscribed.number("status") == 200 && s.device() == "x" &&
         s.interval_ms() == 500);
  auto bad = s.receive(
      Json::parse(
          R"({"id":"bad","type":"subscribe","device_id":"../","interval_ms":1})"),
      8);
  assert(bad.number("status") == 400 && s.device() == "x");
  auto calls_before = calls;
  assert(s.receive(Json::parse(R"({"type":"ping"})"), 9).number("status") ==
             400 &&
         calls == calls_before);

  unsigned accepted = 0;
  StreamSession crowded(
      "boot", [](const std::string &) { return true; },
      [&](const std::string &, const std::string &, const Json &,
          const Headers &) {
        return Response{202,
                        Json().set("id", "op-" + std::to_string(++accepted))};
      });
  crowded.receive(Json::parse(R"({"type":"auth","token":"secret"})"), 0);
  for (unsigned n = 0; n < 17; ++n) {
    auto result = crowded.receive(
        Json::parse(R"({"method":"POST","path":"/api/v1/fixtures","body":{}})"),
        1);
    assert(result.number("status") == 202);
  }
  assert(crowded.operations().size() == 16 &&
         crowded.operations().front() == "op-1");
  assert(crowded.closing_required()); // Accepted reply precedes overload close.
  assert(
      crowded
          .receive(
              Json::parse(R"({"method":"POST","path":"/api/v1/fixtures"})"), 2)
          .number("status") == 503);
  assert(accepted == 17);

  assert(!s.authentication_expired(10000));
  StreamSession unauth(
      "boot-b", [](const std::string &) { return false; }, dispatch);
  assert(unauth.authentication_expired(3001));
  auto request =
      Json::parse(R"({"id":"x","method":"DELETE","path":"http://remote/"})");
  assert(s.receive(request, 6).number("status") == 400);
}
