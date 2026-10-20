#include "rd60xx_driver.hpp"
#include "rd_wifi.hpp"
#include <cassert>
using namespace labbridge;
using namespace labbridge::rd60xx;
static Bytes reply(uint32_t serial, uint16_t model = 60181) {
  std::array<uint16_t, 20> r{};
  r[0] = model;
  r[1] = serial >> 16;
  r[2] = serial;
  r[3] = 136;
  r[8] = 420;
  r[9] = 15;
  r[10] = 410;
  r[11] = 10;
  r[13] = 41;
  Bytes b{1, 3, 40};
  for (auto v : r) {
    b.push_back(v >> 8);
    b.push_back(v);
  }
  auto c = crc(b.data(), b.size());
  b.push_back(c);
  b.push_back(c >> 8);
  return b;
}
int main() {
  std::array<uint16_t, 20> r{};
  r[0] = 60181;
  r[2] = 123;
  r[9] = 15;
  assert(decode(r, 6018).set_current == .15);
  bool rejected = false;
  try {
    decode(r, 6024);
  } catch (...) {
    rejected = true;
  }
  assert(rejected);
  assert(profile(6018).max_current == 18);
  r[0] = 60185;
  rejected = false;
  try {
    decode(r, 6018);
  } catch (...) {
    rejected = true;
  }
  assert(rejected);
  RtuStream parser;
  auto b = reply(123);
  Bytes first(b.begin(), b.begin() + 7), last(b.begin() + 7, b.end());
  assert(parser.feed(first).empty());
  auto frames = parser.feed(last);
  assert(frames.size() == 1 && frames[0] == b);
  auto corrupt = b;
  corrupt[12] ^= 1;
  corrupt.insert(corrupt.end(), b.begin(), b.end());
  auto echo = write(1, 18, 0);
  corrupt.insert(corrupt.end(), echo.begin(), echo.end());
  frames = parser.feed(corrupt);
  assert(frames.size() == 2 && frames[0] == b && frames[1] == echo);
  parser.clear();
  auto bogus = Bytes{1, 3, 250};
  bogus.insert(bogus.end(), b.begin(), b.end());
  frames = parser.feed(bogus);
  assert(frames.size() == 1 && frames[0] == b);
  for (int i = 0; i < 100; i++)
    parser.feed(Bytes(100, 0xff));
  assert(parser.buffered() < 256);
  auto identity = identify(b);
  assert(identity.serial == 123 && identity.model == 6018 &&
         identity.model_id == 60181 && identity.firmware == 136);
  // Official ESPTouch guide lengths; independently decoded payload checks
  // below.
  auto packets = esptouch_lengths("LabBridge-Setup", "", {192, 168, 4, 1},
                                  {0, 1, 2, 3, 4, 5});
  assert(packets.guide == std::vector<uint16_t>({515, 514, 513, 512}));
  assert(packets.data.size() >= 3);
  assert(packets.data[0] == 185 && packets.data[1] == 296 &&
         packets.data[2] == 208);
  // Each encoded datum has low/high nibble + sequence triplet; no password
  // bytes.
  std::map<int, int> decoded;
  for (size_t i = 0; i < packets.data.size(); i += 3) {
    auto a = packets.data[i] - 40, seq = packets.data[i + 1] - 40,
         z = packets.data[i + 2] - 40;
    assert((seq & 0xff00) == 0x100);
    decoded[seq & 255] = ((a & 15) << 4) | (z & 15);
  }
  int checksum = 0;
  for (int i = 0; i < 24; ++i)
    checksum ^= decoded[i];
  assert(checksum == 0);
  assert(decoded[0] == 24 && decoded[1] == 0 && decoded[5] == 192 &&
         decoded[8] == 1 && decoded[9] == 'L');
  // The radio path must preserve every UDP payload length, use our AP BSSID
  // and produce a decodable open-network FromDS broadcast (no station needed).
  const std::array<uint8_t, 6> mac{2, 1, 2, 3, 4, 5};
  for (auto length : {40, 296, 512, 515}) {
    auto frame = esptouch_frame(length, mac);
    assert(frame.size() == size_t(60 + length));
    assert(frame[0] == 8 && frame[1] == 2);
    for (int i = 0; i < 6; ++i) {
      assert(frame[4 + i] == 255);
      assert(frame[10 + i] == mac[i] && frame[16 + i] == mac[i]);
    }
    assert(frame[24] == 0xaa && frame[30] == 8 && frame[31] == 0);
    auto word = [&](size_t offset) {
      return (unsigned(frame[offset]) << 8) | frame[offset + 1];
    };
    assert(frame[32] == 0x45 && word(34) == unsigned(28 + length));
    assert(frame[41] == 17 && frame[44] == 192 && frame[47] == 1);
    assert(word(48) == 0xffff && word(50) == 0xffff);
    unsigned sum = 0;
    for (int i = 32; i < 52; i += 2)
      sum += word(i);
    while (sum >> 16)
      sum = (sum & 0xffff) + (sum >> 16);
    assert(sum == 0xffff);
    assert(word(54) == 7001 && word(56) == unsigned(8 + length));
    assert(word(58) == 0); // IPv4 UDP checksum may be omitted.
    for (size_t i = 60; i < frame.size(); ++i)
      assert(frame[i] == 1);
  }
  for (auto length : {0, 39, 516}) {
    rejected = false;
    try {
      esptouch_frame(length, mac);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    assert(rejected);
  }
  IdentityBindings bindings;
  bindings.configure(123, 6018);
  bindings.configure(456, 6018);
  assert(bindings.claim(identify(reply(456)), 11) ==
         IdentityBindings::Claim::bound);
  assert(bindings.claim(identify(reply(123)), 12) ==
         IdentityBindings::Claim::bound);
  assert(bindings.claim(identify(reply(123)), 13) ==
         IdentityBindings::Claim::rejected);
  assert(bindings.claim(identify(reply(789)), 14) ==
         IdentityBindings::Claim::discovered);
  assert(bindings.claim(identify(reply(123, 60241)), 15) ==
         IdentityBindings::Claim::rejected);
  bindings.release(12);
  assert(bindings.claim(identify(reply(123)), 16) ==
         IdentityBindings::Claim::bound);
  auto discovered = bindings.describe();
  assert(cJSON_GetArraySize(discovered.get()) == 3);
  Provisioning session;
  assert(session.status(0).text("state") == "idle");
  session.start("boot-p1", 0);
  assert(session.status(1).text("state") == "server_address");
  rejected = false;
  try {
    session.advance("wrong", "server_address", 2);
  } catch (...) {
    rejected = true;
  }
  assert(rejected);
  session.advance("boot-p1", "server_address", 3);
  assert(session.status(3).text("state") == "wifi");
  // Replayed advance cannot skip the Wi-Fi phase.
  session.advance("boot-p1", "server_address", 4);
  assert(session.status(4).text("state") == "wifi");
  session.advance("boot-p1", "wifi", 5);
  assert(session.status(5).text("state") == "waiting_connection");
  session.connected(identity);
  assert(session.status(6).text("state") == "complete");
  session.start("boot-p2", 10);
  session.tick(120010);
  assert(session.status(120010).text("state") == "timed_out");
  session.start("boot-p3", 120020);
  session.cancel("boot-p3");
  session.cancel("boot-p3");
  assert(session.status(120030).text("state") == "cancelled");
  // Physical profiles constrain configuration before a transport is allocated.
  rejected = false;
  try {
    profile(6018).validate({60, 24, 1000, 3000, 0});
  } catch (...) {
    rejected = true;
  }
  assert(rejected);
}
