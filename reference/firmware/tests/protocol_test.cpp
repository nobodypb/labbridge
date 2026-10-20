#include "protocol.hpp"
#include <cassert>
#include <iostream>
using namespace lab;
int main() {
  auto request = modbus_read(0, 20);
  assert(request == Bytes({1, 3, 0, 0, 0, 20, 0x45, 0xc5}));
  auto write = modbus_write(18, 0);
  assert(modbus_reply(write, 6, 0));
  write[5] = 1;
  assert(!modbus_reply(write, 6, 0));
  Bytes read{1, 3, 4, 0, 0, 1, 0};
  auto crc = crc16(read.data(), read.size());
  read.push_back(crc & 255);
  read.push_back(crc >> 8);
  assert(modbus_reply(read, 3, 4));
  assert(!modbus_reply(read, 3, 2));
  assert(px_current(1.29) == Bytes({0xb1, 0xb2, 2, 1, 29, 0xb6}));
  assert(px_current(NAN).empty());
  assert(px_current(-1).empty());
  assert(px3_command(0x10) == Bytes({0xb1, 0xb2, 0x10, 0, 0, 0, 0, 0, 0xb6}));
  uint32_t value = 0;
  Bytes v3{0xca, 0xcb, 0x11, 0, 0x5d, 0x5c, 0, 0, 0xce, 0xcf};
  assert(px3_reply(v3, 0x11, value) && value == 23900);
  assert(!px3_reply(v3, 0x10, value));
  v3.pop_back();
  assert(!px3_reply(v3, 0x11, value));
  assert(spp_command(2, 100) ==
         Bytes({0xb1, 0xb2, 2, 0, 0, 100, 0, 0, 0xb6}));
  Bytes spp_answer{0xca, 0xcb, 0x17, 0, 0, 100, 0, 0, 0xce, 0xcf};
  assert(spp_reply(spp_answer, 0x17, value) && value == 100);
  assert(!spp_reply(spp_answer, 0x10, value));
  Bytes report(36);
  report[0] = 255;
  report[1] = 85;
  report[2] = 1;
  report[3] = 2;
  report[6] = 120;
  report[8] = 3;
  report[9] = 232;
  report[25] = 25;
  uint8_t sum = 0;
  for (int i = 2; i < 35; i++)
    sum += report[i];
  report[35] = sum ^ 0x44;
  Reading r;
  assert(atorch_report(report, false, r));
  assert(r.voltage == 12 && r.current == 1 && r.power == 12);
  for (size_t cut = 0; cut <= 36; cut++) {
    AtorchStream s;
    auto a = s.feed(report.data(), cut);
    auto b = s.feed(report.data() + cut, 36 - cut);
    assert(a.size() + b.size() == 1);
  }
  AtorchStream s;
  Bytes joined{0, 1, 0xff, 0, 0x6f};
  joined.insert(joined.end(), report.begin(), report.end());
  joined.insert(joined.end(), {0xca, 0xcb, 0, 1, 2, 0xce, 0xcf});
  assert(s.feed(joined.data(), joined.size()).size() == 3);
  AtorchStream spp_stream;
  for (size_t cut = 0; cut <= spp_answer.size(); ++cut) {
    spp_stream.clear();
    auto a = spp_stream.feed(spp_answer.data(), cut);
    auto b = spp_stream.feed(spp_answer.data() + cut,
                             spp_answer.size() - cut);
    assert(a.size() + b.size() == 1);
    assert((a.empty() ? b.front() : a.front()) == spp_answer);
  }
  // Captured V2.0 UART interleaving: the reply begins inside a damaged
  // telemetry frame. Exercise every BLE notification split, including a
  // reply extending past the nominal 36-byte report boundary.
  auto unhex = [](const std::string &text) {
    Bytes bytes;
    for (size_t i = 0; i < text.size(); i += 2)
      bytes.push_back(std::stoul(text.substr(i, 2), nullptr, 16));
    return bytes;
  };
  std::vector<Bytes> interrupted{
      unhex("ff5501020000f00000000000fa000000cacb170000640000cecf"
            "0000000000000000000000000000000000000000"),
      unhex("ff5501020000f0cacb120000000000cecf00cecf"
            "0000000000000000000000000000000000000000000000000000")};
  Bytes boundary(report.begin(), report.begin() + 33);
  boundary.insert(boundary.end(), spp_answer.begin(), spp_answer.end());
  interrupted.push_back(boundary);
  for (auto captured : interrupted) {
    captured.insert(captured.end(), report.begin(), report.end());
    for (size_t cut = 0; cut <= captured.size(); ++cut) {
      AtorchStream stream;
      auto frames = stream.feed(captured.data(), cut);
      auto tail = stream.feed(captured.data() + cut, captured.size() - cut);
      frames.insert(frames.end(), tail.begin(), tail.end());
      assert(frames.size() == 2);
      assert(frames[0].size() == 10 && frames[0][0] == 0xca);
      assert(frames[1] == report); // damaged telemetry was discarded
    }
    AtorchStream stream;
    std::vector<Bytes> frames;
    for (auto byte : captured) {
      auto part = stream.feed(&byte, 1);
      frames.insert(frames.end(), part.begin(), part.end());
    }
    assert(frames.size() == 2 && frames[1] == report);
  }
  auto lookalike = report;
  std::copy(spp_answer.begin(), spp_answer.end(), lookalike.begin() + 13);
  sum = 0;
  for (int i = 2; i < 35; ++i)
    sum += lookalike[i];
  lookalike[35] = sum ^ 0x44;
  AtorchStream intact;
  auto intact_frames = intact.feed(lookalike.data(), lookalike.size());
  assert(intact_frames.size() == 1 && intact_frames[0] == lookalike);
  report[35] ^= 1;
  assert(!atorch_report(report, false, r));
  assert(atorch_report(report, true, r));
  report[7] = 255;
  assert(!atorch_report(report, true, r));
  Limits l{12, 2, 12};
  assert(l.contains(12, 1));
  assert(!l.contains(12, 2));
  assert(!l.contains(NAN, 0));
  Lease lease;
  lease.token = "one";
  lease.deadline = 100;
  assert(lease.owns("one", 99));
  assert(!lease.owns("two", 99));
  assert(!lease.owns("one", 100));
  lease.stopping = true;
  assert(!lease.owns("one", 1));
  std::cout << "protocol and lease tests passed\n";
}
