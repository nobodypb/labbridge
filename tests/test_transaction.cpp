#include "labbridge/transaction.hpp"
#include <cassert>
using namespace labbridge;
struct Wire : Endpoint {
  bool live = true, can_write = true;
  uint64_t gen = 1;
  Bytes rx;
  std::vector<Bytes> sent;
  bool connected() const override { return live; }
  bool writable() const override { return can_write; }
  uint64_t generation() const override { return gen; }
  bool send(const Bytes &b) override {
    sent.push_back(b);
    return live;
  }
  Bytes receive() override {
    Bytes b;
    b.swap(rx);
    return b;
  }
};
static Bytes reply(uint8_t c, uint32_t v) {
  return {0xca, 0xcb, c,   uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v), 0,
          0,    0xce, 0xcf};
}
int main() {
  Wire w;
  Transaction t(w);
  t.start({{2, 100, 0x17, 100, 60}}, 0, false);
  t.tick(59);
  assert(w.sent.empty());
  t.tick(60);
  assert(w.sent.size() == 1 && w.sent[0][2] == 2);
  t.tick(61);
  assert(w.sent.size() == 2 && w.sent[1][2] == 0x17);
  w.rx = reply(0x17, 100);
  t.tick(62);
  assert(t.result().state == TxState::succeeded);
  // Missing application execution is fixed locally, with no permanent 500-ms
  // tax.
  Transaction r(w);
  r.start({{2, 150, 0x17, 150, 60}}, 100, false);
  r.tick(160);
  r.tick(161);
  w.rx = reply(0x17, 100);
  r.tick(162);
  r.tick(361);
  auto n = w.sent.size();
  r.tick(362);
  assert(w.sent.size() == n + 1);
  r.tick(363);
  w.rx = reply(0x17, 150);
  r.tick(364);
  assert(r.result().state == TxState::succeeded &&
         r.result().write_attempts == 2);
  // A connection generation change must never replay ON.
  Transaction on(w);
  on.start({{1, 0x010000, 0x10, 1, 0}}, 400, true);
  on.tick(400);
  ++w.gen;
  on.tick(401);
  assert(on.result().state == TxState::failed &&
         on.result().reason == "connection_changed");
  Transaction timeout(w);
  timeout.start({{0, 0, 0x11, {}, 0}}, 500, false);
  for (uint64_t now = 500; now <= 11000; now += 10)
    timeout.tick(now);
  assert(timeout.result().state == TxState::failed &&
         timeout.result().read_attempts <= 9);
  // Cancellation discards transaction context; the separate OFF transaction
  // still works.
  Transaction cancel(w);
  cancel.start({{2, 100, 0x17, 100, 60}}, 12000, false);
  cancel.cancel();
  cancel.tick(12100);
  assert(cancel.result().state == TxState::cancelled);
  // A native reply can outlive the first read timer while BLE is still busy
  // completing ATT. Accept it during backoff before any new query/write is
  // sent.
  Wire delayed;
  Transaction late(delayed);
  late.start({{2, 150, 0x17, 150, 0}}, 0, false);
  late.tick(0);
  late.tick(1);
  delayed.can_write = false;
  late.tick(161);
  delayed.rx = reply(0x17, 150);
  late.tick(185);
  assert(late.result().state == TxState::succeeded);
  assert(late.result().read_attempts == 1 && late.result().write_attempts == 1);
  assert(delayed.sent.size() == 2);
  Wire invalidated;
  Transaction stale_reply(invalidated);
  stale_reply.start({{0, 0, 0x11, {}, 0}}, 0, false);
  stale_reply.tick(0);
  stale_reply.tick(160);
  ++invalidated.gen;
  invalidated.rx = reply(0x11, 24000);
  stale_reply.tick(185);
  assert(stale_reply.result().state == TxState::failed);
  // An uncertain activation is never repeated. Read-only verification may
  // continue, but neither absent replies nor OFF readback permits a second ON.
  Wire silent_on;
  Transaction uncertain_on(silent_on);
  uncertain_on.start({{1, 0x010000, 0x10, 1, 0}}, 0, true);
  for (uint64_t clock = 0; clock < 4000; ++clock)
    uncertain_on.tick(clock);
  assert(uncertain_on.result().state == TxState::failed);
  assert(uncertain_on.result().write_attempts == 1);
  Wire protected_on;
  Transaction rejected_on(protected_on);
  rejected_on.start({{1, 0x010000, 0x10, 1, 0}}, 0, true);
  for (uint64_t clock = 0; clock < 4000; ++clock) {
    protected_on.rx = reply(0x10, 0);
    rejected_on.tick(clock);
  }
  assert(rejected_on.result().state == TxState::failed);
  assert(rejected_on.result().write_attempts == 1);
}
