#include "codec.hpp"
#include <algorithm>
#include <cassert>
#include <iostream>
using namespace labbridge::dl24m;
int main() {
  assert(px3_command(2, 100) == Bytes({0xb1, 0xb2, 2, 0, 0, 100, 0, 0, 0xb6}));
  assert(px3_command(1, 0x010000) ==
         Bytes({0xb1, 0xb2, 1, 1, 0, 0, 0, 0, 0xb6}));
  assert(px3_command(2, 0x1000000).empty());
  Bytes reply{0xca, 0xcb, 0x17, 0, 0, 100, 0, 0, 0xce, 0xcf};
  uint32_t value = 0;
  assert(px3_reply(reply, 0x17, value) && value == 100);
  assert(!px3_reply(reply, 0x10, value));
  Bytes telemetry(36);
  telemetry[0] = 255;
  telemetry[1] = 85;
  telemetry[2] = 1;
  telemetry[3] = 2;
  telemetry[6] = 120;
  telemetry[25] = 25;
  uint8_t sum = 0;
  for (int i = 2; i < 35; ++i)
    sum += telemetry[i];
  telemetry[35] = sum ^ 0x44;
  Reading reading;
  assert(atorch_report(telemetry, false, reading));
  assert(reading.voltage == 12);
  Bytes interrupted(telemetry.begin(), telemetry.begin() + 18);
  interrupted.insert(interrupted.end(), reply.begin(), reply.end());
  interrupted.insert(interrupted.end(), telemetry.begin(), telemetry.end());
  for (size_t cut = 0; cut <= interrupted.size(); ++cut) {
    AtorchStream parser;
    auto frames = parser.feed(interrupted.data(), cut);
    auto tail = parser.feed(interrupted.data() + cut, interrupted.size() - cut);
    frames.insert(frames.end(), tail.begin(), tail.end());
    assert(frames.size() == 2 && frames[0] == reply && frames[1] == telemetry);
  }
  AtorchStream first, second;
  assert(first.feed(reply.data(), 5).empty());
  assert(second.feed(reply.data(), reply.size()).size() == 1);
  assert(first.feed(reply.data() + 5, reply.size() - 5).size() == 1);
  assert(dl24m_reads.size() == 22 && !dl24m_read_only(0x22) &&
         !dl24m_read_only(0x31));
  std::cout
      << "DL24M framing, interleaving and independent parser tests passed\n";
}
