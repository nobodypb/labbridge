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
