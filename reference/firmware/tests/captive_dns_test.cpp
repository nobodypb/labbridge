#include "captive_dns.hpp"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using lab::captive_dns_reply;
using Bytes = std::vector<uint8_t>;

static Bytes query(uint16_t type) {
  Bytes q{0x12,
          0x34,
          1,
          0,
          0,
          1,
          0,
          0,
          0,
          0,
          0,
          1,
          7,
          'e',
          'x',
          'a',
          'm',
          'p',
          'l',
          'e',
          3,
          'o',
          'r',
          'g',
          0,
          uint8_t(type >> 8),
          uint8_t(type),
          0,
          1};
  // One EDNS additional record, as sent by many clients.
  q.insert(q.end(), {0, 0, 41, 4, 0, 0, 0, 0, 0, 0, 0});
  return q;
}
int main() {
  std::array<uint8_t, 4> ip{192, 168, 4, 1};
  std::array<uint8_t, 512> reply{};
  size_t len = 0;
  auto q = query(1);
  assert(captive_dns_reply(q.data(), q.size(), ip.data(), reply.data(),
                           reply.size(), len));
  assert(len == 29 + 16); // EDNS is stripped, not placed before the answer.
  assert(reply[0] == 0x12 && reply[1] == 0x34);
  assert(reply[2] == 0x85 && reply[3] == 0 && reply[5] == 1);
  assert(reply[7] == 1 && reply[11] == 0);
  assert(reply[29] == 0xc0 && reply[30] == 0x0c);
  assert(reply[len - 4] == 192 && reply[len - 3] == 168 &&
         reply[len - 2] == 4 && reply[len - 1] == 1);
  q = query(28);
  assert(captive_dns_reply(q.data(), q.size(), ip.data(), reply.data(),
                           reply.size(), len));
  assert(len == 29 && reply[7] == 0); // AAAA: empty, so IPv4 is used.
  q = query(1);
  q[12] = 0xc0; // Compressed labels are rejected.
  assert(!captive_dns_reply(q.data(), q.size(), ip.data(), reply.data(),
                            reply.size(), len));
  q = query(1);
  q[5] = 2; // Multiple questions cannot be answered as one.
  assert(!captive_dns_reply(q.data(), q.size(), ip.data(), reply.data(),
                            reply.size(), len));
  q = query(1);
  q[2] = 0x81; // Already a response.
  assert(!captive_dns_reply(q.data(), q.size(), ip.data(), reply.data(),
                            reply.size(), len));
  q = query(1);
  assert(
      !captive_dns_reply(q.data(), q.size(), ip.data(), reply.data(), 32, len));
  std::cout << "captive DNS tests passed\n";
}
