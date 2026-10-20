#include "rd60xx.hpp"
#include <cassert>
using namespace labbridge::rd60xx;
int main() {
  assert(read(1, 0, 20) == Bytes({1, 3, 0, 0, 0, 20, 0x45, 0xc5}));
  auto w = write(1, 18, 0);
  assert(valid(w, 1, 6));
  w[4] ^= 1;
  assert(!valid(w, 1, 6));
  std::array<uint16_t, 20> r{};
  r[0] = 60241;
  r[1] = 1;
  r[2] = 2;
  r[8] = 420;
  r[9] = 100;
  r[10] = 410;
  r[11] = 50;
  r[13] = 205;
  r[17] = 1;
  r[18] = 1;
  auto m = decode(r);
  assert(m.serial == 65538 && m.current == .5 && m.voltage == 4.1 &&
         m.power == 2.05 && m.output && m.cc);
  r[0] = 60061;
  bool fail = false;
  try {
    decode(r);
  } catch (...) {
    fail = true;
  }
  assert(fail);
}
