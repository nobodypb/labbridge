#include "labbridge/interlock.hpp"
#include <cassert>
using namespace labbridge;
struct Pins : SwitchIo {
  bool c = false, d = false, co = false, do_ = false;
  void drive(bool a, bool b) override {
    co = a;
    do_ = b;
  }
  bool charge_closed() const override { return c; }
  bool discharge_closed() const override { return d; }
};
int main() {
  Pins pins;
  Interlock s(pins, 20);
  s.select(Path::charge, 0);
  s.tick(20);
  pins.c = true;
  s.tick(25);
  assert(s.path() == Path::charge);
  s.stop(30);
  s.tick(31);
  assert(!s.faulted() && !s.confirmed());
  pins.c = false;
  s.tick(40);
  assert(s.confirmed() && s.path() == Path::isolated);
  s.select(Path::discharge, 50);
  s.tick(70);
  s.tick(571);
  assert(s.faulted() && !pins.co && !pins.do_);
}
