#pragma once
#include <cstddef>
#include <cstdint>

namespace labbridge {
// Respond to one ordinary DNS question with the setup AP address. The parser
// ignores optional EDNS records and does not reflect untrusted packet tails.
bool captive_dns_reply(const uint8_t *query, size_t query_len,
                       const uint8_t ap_ip[4], uint8_t *reply,
                       size_t reply_capacity, size_t &reply_len);
} // namespace labbridge
