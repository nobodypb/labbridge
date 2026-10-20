#include "captive_dns.hpp"
#include <cstring>

namespace lab {
bool captive_dns_reply(const uint8_t *query, size_t query_len,
                       const uint8_t ap_ip[4], uint8_t *reply,
                       size_t reply_capacity, size_t &reply_len) {
  reply_len = 0;
  if (!query || !ap_ip || !reply || query_len < 17 || query_len > 512 ||
      (query[2] & 0x80) || (query[2] & 0x78) || query[4] != 0 || query[5] != 1)
    return false;
  // The DNS question starts at byte 12. Label compression is unnecessary in
  // a single-question request; reject it rather than follow arbitrary offsets.
  size_t end = 12;
  size_t name_len = 0;
  while (end < query_len && query[end] != 0) {
    uint8_t label = query[end++];
    if (label > 63 || end + label >= query_len || name_len + label + 1 > 253)
      return false;
    end += label;
    name_len += label + 1;
  }
  if (end >= query_len || end + 5 > query_len)
    return false;
  const size_t question_end = end + 5;
  uint16_t type = (uint16_t(query[end + 1]) << 8) | query[end + 2];
  uint16_t klass = (uint16_t(query[end + 3]) << 8) | query[end + 4];
  bool answer = type == 1 && klass == 1; // IN A; AAAA gets an empty reply.
  size_t needed = question_end + (answer ? 16 : 0);
  if (needed > reply_capacity)
    return false;
  memcpy(reply, query, question_end);
  reply[2] = 0x84 | (query[2] & 1); // response, authoritative, preserve RD
  reply[3] = 0;
  reply[6] = 0;
  reply[7] = answer ? 1 : 0;
  reply[8] = reply[9] = reply[10] = reply[11] = 0;
  if (answer) {
    static const uint8_t rr[] = {0xc0, 0x0c, 0, 1, 0, 1, 0,
                                 0,    0,    0, 0, 4}; // TTL 0, IPv4 length 4
    memcpy(reply + question_end, rr, sizeof(rr));
    memcpy(reply + question_end + sizeof(rr), ap_ip, 4);
  }
  reply_len = needed;
  return true;
}
} // namespace lab
