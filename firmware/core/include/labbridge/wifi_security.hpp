#pragma once
#include <algorithm>
#include <string>
namespace labbridge {
inline bool valid_ap_passphrase(const std::string &s) {
  return s.empty() || (s.size() >= 8 && s.size() <= 63 &&
                       std::all_of(s.begin(), s.end(), [](unsigned char c) {
                         return c >= 32 && c <= 126;
                       }));
}
} // namespace labbridge
