#include "dl24m_driver.hpp"
#include "rd60xx_driver.hpp"
#include <cassert>
using namespace labbridge;
struct Delayed : Endpoint {
  uint64_t now = 0, due = 0;
  bool pending = false;
  unsigned overlaps = 0;
  Bytes reply;
  std::map<uint8_t, uint32_t> regs{{0x10, 0},   {0x11, 24000}, {0x12, 0},
                                   {0x16, 250}, {0x17, 100},   {0x21, 0}};
  bool connected() const { return true; }
  uint64_t generation() const { return 1; }
  bool send(const Bytes &b) {
    if (pending) {
      ++overlaps;
      return false;
    }
    auto c = b[2];
    if (c == 1)
      regs[0x10] = b[3] ? 1 : 0;
    else if (c == 2)
      regs[0x17] = dl24m::be24(b.data() + 3);
    else if (c == 0x31 || dl24m::dl24m_read_only(c)) {
      auto v = regs[c];
      reply = {
          0xca, 0xcb, c,   uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v), 0,
          0,    0xce, 0xcf};
      pending = true;
      due = now + 50;
    }
    return true;
  }
  Bytes receive() {
    if (!pending || now < due)
      return {};
    pending = false;
    Bytes b;
    b.swap(reply);
    return b;
  }
};
struct DelayedRtu : Endpoint {
  uint64_t now = 0, due = 0;
  uint64_t response_delay = 50;
  bool pending = false;
  unsigned overlaps = 0;
  Bytes rx;
  std::array<uint16_t, 42> regs{};
  DelayedRtu() {
    regs[0] = 60241;
    regs[2] = 123;
    regs[8] = 420;
    regs[9] = 100;
  }
  bool connected() const { return true; }
  uint64_t generation() const { return 1; }
  bool send(const Bytes &b) {
    if (pending) {
      ++overlaps;
      return false;
    }
    if (b[1] == 3) {
      unsigned first = (b[2] << 8) | b[3], count = (b[4] << 8) | b[5];
      assert(first + count <= regs.size());
      rx = {1, 3, uint8_t(count * 2)};
      for (unsigned i = first; i < first + count; ++i) {
        auto r = regs[i];
        rx.push_back(r >> 8);
        rx.push_back(r);
      }
      auto crc = rd60xx::crc(rx.data(), rx.size());
      rx.push_back(crc);
      rx.push_back(crc >> 8);
    } else {
      regs[(b[2] << 8) | b[3]] = (b[4] << 8) | b[5];
      rx = b;
    }
    pending = true;
    due = now + response_delay;
    return true;
  }
  Bytes receive() {
    if (!pending || now < due)
      return {};
    pending = false;
    Bytes b;
    b.swap(rx);
    return b;
  }
};
int main() {
  // LB-019: a reply exceeding 500 ms is latency, not a fixed pause required
  // between successful commands. Initial OFF and a checked write still finish
  // within the operation deadline, without overlapping outstanding requests.
  DelayedRtu slow;
  slow.response_delay = 600;
  Rd60xxDriver slow_source(
      "source", 123, slow, {5, 1, 5, 3000, 0}, [](const Json &) {}, Json());
  for (; slow.now < 3500; slow.now += 5)
    slow_source.tick(slow.now);
  assert(slow_source.describe(slow.now).text("state") == "ready");
  slow_source.execute("setSourceVoltage", Json::parse(R"({"voltage_v":4.1})"),
                      slow.now);
  for (auto end = slow.now + 3000; slow.now < end; slow.now += 5)
    slow_source.tick(slow.now);
  assert(slow.overlaps == 0 && slow.regs[8] == 410 &&
         slow_source.operation(slow.now).text("status") == "succeeded");
  Delayed w;
  Dl24mDriver d(
      "load", "fake", w, {25, .2, 5, 3000, 0}, [](const Json &) {}, Json());
  for (; w.now < 1000; w.now += 5)
    d.tick(w.now);
  while (!w.pending) {
    d.tick(w.now);
    w.now += 5;
  }
  auto start = w.now;
  d.execute("setLoadCurrent", Json::parse(R"({"current_a":0.15})"), w.now);
  for (; w.now < start + 2000; w.now += 5)
    d.tick(w.now);
  assert(w.overlaps == 0);
  assert(d.operation(w.now).text("status") == "succeeded");
  assert(w.regs[0x17] == 150);
  d.stop(w.now);
  for (auto end = w.now + 1000; w.now < end; w.now += 5)
    d.tick(w.now);
  assert(w.overlaps == 0 && d.operation(w.now).text("status") == "succeeded");
  DelayedRtu rtu;
  Rd60xxDriver source(
      "source", 123, rtu, {5, 1, 5, 3000, 0}, [](const Json &) {}, Json());
  for (; rtu.now < 1000; rtu.now += 5)
    source.tick(rtu.now);
  while (!rtu.pending) {
    source.tick(rtu.now);
    rtu.now += 5;
  }
  auto begun = rtu.now;
  source.execute("setSourceVoltage", Json::parse(R"({"voltage_v":4.1})"),
                 rtu.now);
  for (; rtu.now < begun + 2000; rtu.now += 5)
    source.tick(rtu.now);
  assert(rtu.overlaps == 0 && rtu.regs[8] == 410 &&
         source.operation(rtu.now).text("status") == "succeeded");
  source.stop(rtu.now);
  for (auto end = rtu.now + 1000; rtu.now < end; rtu.now += 5)
    source.tick(rtu.now);
  assert(rtu.overlaps == 0);
  Node node("node", "boot", "sim");
  node.add("load", std::shared_ptr<Driver>(&d, [](Driver *) {}));
  auto lease =
      node.request("POST", "/api/v1/leases",
                   Json::parse(R"({"device_ids":["load"]})"), {}, w.now);
  Headers headers{{"X-Lab-Lease", lease.body.text("id")},
                  {"Idempotency-Key", "enable"}};
  auto on = node.request("PUT", "/api/v1/devices/load/output",
                         Json::parse(R"({"enabled":true})"), headers, w.now);
  assert(on.status == 202);
  for (auto end = w.now + 1000; w.now < end; w.now += 5)
    node.tick(w.now);
  assert(node.request("GET", "/api/v1/operations/" + on.body.text("id"), Json(),
                      {}, w.now)
             .body.text("status") == "succeeded");
  auto off =
      node.request("POST", "/api/v1/devices/load/stop", Json::parse("{}"),
                   {{"Idempotency-Key", "stop"}}, w.now);
  assert(off.status == 202);
  for (auto end = w.now + 1000; w.now < end; w.now += 5)
    node.tick(w.now);
  assert(node.request("GET", "/api/v1/operations/" + off.body.text("id"),
                      Json(), {}, w.now)
             .body.text("status") == "succeeded");
  assert(node.request("GET", "/api/v1/leases/" + lease.body.text("id"), Json(),
                      {}, w.now)
             .body.text("state") == "active");
}
