#pragma once
#include "labbridge/auth.hpp"
#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <cstring>
#include <sys/socket.h>

namespace labbridge::http {
inline AuthContext context(const sockaddr_storage &address, socklen_t length,
                           std::string host, std::string hostname) {
  char ip[INET6_ADDRSTRLEN];
  const char *decoded = nullptr;
  if (address.ss_family == AF_INET && length >= sizeof(sockaddr_in)) {
    sockaddr_in ipv4{};
    std::memcpy(&ipv4, &address, sizeof(ipv4));
    decoded = inet_ntop(AF_INET, &ipv4.sin_addr, ip, sizeof(ip));
  } else if (address.ss_family == AF_INET6 && length >= sizeof(sockaddr_in6)) {
    sockaddr_in6 ipv6{};
    std::memcpy(&ipv6, &address, sizeof(ipv6));
    if (IN6_IS_ADDR_V4MAPPED(&ipv6.sin6_addr))
      decoded = inet_ntop(AF_INET, &ipv6.sin6_addr.s6_addr[12], ip, sizeof(ip));
    else
      decoded = inet_ntop(AF_INET6, &ipv6.sin6_addr, ip, sizeof(ip));
  }
  if (!decoded)
    return {};
  for (auto *name : {&host, &hostname})
    std::transform(name->begin(), name->end(), name->begin(),
                   [](unsigned char c) { return std::tolower(c); });
  std::string literal = std::strchr(ip, ':') ? "[" + std::string(ip) + "]" : ip;
  bool recognized =
      !host.empty() &&
      (host == literal || host == literal + ":80" ||
       (!hostname.empty() && (host == hostname || host == hostname + ":80")));
  return {false, std::string(ip) == "192.168.4.1",
          recognized ? "http://" + host : ""};
}
} // namespace labbridge::http
