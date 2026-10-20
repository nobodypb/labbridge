#include "labbridge/interlock.hpp"
#include <cassert>
#include <iostream>
using namespace labbridge;
struct Io : SwitchIo {
  bool charge = false, discharge = false, stuck = false;
  void drive(bool c, bool d) override {
    assert(!(c && d));
    charge = c;
    discharge = d;
  }
  bool charge_closed() const override { return charge || stuck; }
  bool discharge_closed() const override { return discharge; }
};
int main() {
  Io io;
  Interlock sw(io, 20);
  assert(!io.charge && !io.discharge);
  assert(sw.select(Path::charge, 0));
  sw.tick(19);
  assert(!io.charge);
  sw.tick(20);
  assert(io.charge && !io.discharge && sw.path() == Path::charge);
  assert(sw.select(Path::discharge, 30));
  assert(!io.charge && !io.discharge);
  sw.tick(49);
  assert(!io.discharge);
  sw.tick(50);
  assert(io.discharge && !io.charge);
  sw.stop();
  assert(!io.charge && !io.discharge && sw.path() == Path::isolated);
  io.stuck = true;
  assert(sw.select(Path::charge, 70));
  sw.tick(90);
  assert(sw.faulted() && !io.discharge && !io.charge);
  std::cout << "break-before-make, feedback and fail-open tests passed\n";
}
