#include "labbridge/stream.hpp"
#include <cassert>
using namespace labbridge;
struct Actor : Driver {
  unsigned commands = 0, stops = 0, ticks = 0, descriptions = 0;
  Json describe(uint64_t) override {
    ++descriptions;
    return Json::parse(
        R"({"id":"actor","driver":"fixture","capabilities":["load.cc","output"],"state":"ready","output":"off"})");
  }
  Json measurements(uint64_t now) override {
    return Json()
        .set("device_id", "actor")
        .set("sequence", 1.)
        .set("received_ms", double(now))
        .set("age_ms", 0.)
        .set("quality", "valid")
        .set("temperature_channels", Json::array());
  }
  bool supports(const std::string &op) const override {
    return op == "setLoadCurrent";
  }
  void tick(uint64_t) override { ++ticks; }
  void stop(uint64_t) override { ++stops; }
  Json execute(const std::string &, const Json &, uint64_t) override {
    ++commands;
    return Json::parse(
        R"({"status":"succeeded","stage":"confirmed","read_attempts":1,"write_attempts":1,"reconnects":0,"duration_ms":0,"final_output":"off","restore_pending":false})");
  }
};
int main() {
  Node node("fixture", "boot", "sim");
  auto actor = std::make_shared<Actor>();
  node.add("actor", actor);
  uint64_t now = 0;
  StreamSession session(
      "boot", [](const std::string &t) { return t == "secret"; },
      [&](const std::string &m, const std::string &p, const Json &b,
          const Headers &h) { return node.request(m, p, b, h, now); });
  session.receive(Json::parse(R"({"type":"auth","token":"secret"})"), now);
  auto lease =
      session
          .receive(
              Json::parse(
                  R"({"id":"lease","method":"POST","path":"/api/v1/leases","body":{"device_ids":["actor"],"ttl_ms":1000}})"),
              now)
          .item("body")
          .text("id");
  assert(!lease.empty());
  auto command = Json::parse(
      R"({"id":"set","method":"PUT","path":"/api/v1/devices/actor/load/current","body":{"current_a":0.1}})");
  command.set("headers",
              Json().set("X-Lab-Lease", lease).set("Idempotency-Key", "a"));
  assert(session.receive(command, now).number("status") == 202 &&
         actor->commands == 1);
  assert(session.receive(command, now).number("status") == 202 &&
         actor->commands == 1); // Shared idempotency.
  now = 500;
  auto ticks_before_snapshot = actor->ticks;
  session.offer_snapshot(
      node.stream_snapshot("actor", session.operations(), now));
  assert(actor->ticks ==
         ticks_before_snapshot); // Stream assembly only reads caches.
  auto other = std::make_shared<Actor>();
  node.add("other", other);
  auto compact =
      node.stream_snapshot("actor", session.operations(), now, false, false);
  assert(!compact.has("node") && !compact.has("devices") &&
         !compact.has("measurements"));
  assert(compact.has("actors") && other->descriptions == 0);
  assert(session.operations().size() == 1);
  session.discard_snapshot();
  assert(session.operations().size() ==
         1); // Preempted confirmations must survive.
  session.offer_snapshot(
      node.stream_snapshot("actor", session.operations(), now));
  session.take_snapshot();
  assert(session.operations()
             .empty()); // Retire only the delivered terminal result.
  session.receive(Json().set("type", "ping"), now);
  now = 1001;
  node.tick(now);
  assert(actor->stops == 1); // Neither telemetry nor ping renewed ownership.
  command.item("headers");
  command.set("headers",
              Json().set("X-Lab-Lease", lease).set("Idempotency-Key", "b"));
  assert(session.receive(command, now).number("status") == 423 &&
         actor->commands == 1);
  auto stop = session.receive(
      Json::parse(
          R"({"id":"stop","method":"POST","path":"/api/v1/devices/actor/stop","headers":{"Idempotency-Key":"stop"},"body":{}})"),
      now);
  assert(stop.number("status") == 202 &&
         actor->stops == 2); // Same independent STOP without lease.
}
