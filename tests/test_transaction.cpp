#include "labbridge/transaction.hpp"
#include <cassert>
using namespace labbridge;
struct Wire : Endpoint {
  bool live = true;
  uint64_t gen = 1;
  Bytes rx;
  std::vector<Bytes> sent;
  bool connected() const override { return live; }
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
}
