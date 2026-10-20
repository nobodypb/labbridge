#include "labbridge/temperature.hpp"
#include <cassert>
#include <iostream>
using namespace labbridge;
int main() {
  std::array<uint8_t, 9> s{0x91, 0x01, 0x4b, 0x46, 0x7f,
                           0xff, 0x0c, 0x10, 0x70};
  auto t = decode_ds18b20(s, true);
  assert(t && *t == 25.0625);
  assert(!decode_ds18b20(s, false));
  s[0] ^= 1;
  assert(!decode_ds18b20(s, true));
  TemperatureChannel internal{"load/heatsink", TemperatureKind::internal, 25.0,
                              1000};
  assert(!internal.usable_as_cell(1200, 3000));
  TemperatureChannel external{"probe/cell", TemperatureKind::external_probe,
                              25.0, 1000};
  assert(external.usable_as_cell(1200, 3000));
  assert(!external.usable_as_cell(4001, 3000));
  std::cout << "DS18B20 CRC, conversion and temperature-channel tests passed\n";
}
