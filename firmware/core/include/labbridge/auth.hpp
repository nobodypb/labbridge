#pragma once
#include "node.hpp"
#include <deque>
#include <mutex>

namespace labbridge {
struct AuthContext {
  bool local = false, access_point = false;
  // Trusted server origin, derived from socket address/configured hostname.
  // Empty for an unrecognized Host; never copied from Origin.
  std::string origin;
};
class Auth {
public:
  using Random = std::function<std::string()>;
  using Derive = std::function<std::string(const std::string &,
                                           const std::string &, unsigned)>;
  using Persist = std::function<void(const Json &)>;
  using Clock = std::function<uint64_t()>;
  static constexpr uint64_t session_ttl_ms = 12ULL * 60 * 60 * 1000;
  // Usability-first ESP policy: keep login below three seconds at 160 MHz.
  // This provides basic offline resistance, not a server-strength work factor.
  static constexpr unsigned iterations = 6000;
  static constexpr unsigned verifier_version = 2;
  Auth(std::string bearer, Json record, Random random, Derive derive,
       Persist persist, Clock clock);
  Response authorize(const std::string &method, const std::string &path,
                     const Headers &, const AuthContext &);
  Response request(const std::string &method, const std::string &path,
                   const Json &, const Headers &, const AuthContext &);
  static bool route(const std::string &path) {
    return path.rfind("/api/v1/auth/", 0) == 0;
  }

private:
  struct Session {
    std::string csrf, origin;
    uint64_t deadline;
  };
  std::mutex mutex_;
  std::string bearer_;
  Json record_;
  Random random_;
  Derive derive_;
  Persist persist_;
  Clock clock_;
  bool busy_ = false;
  std::map<std::string, Session> sessions_;
  std::deque<uint64_t> failures_;
  void prune(uint64_t now);
  std::string session_id(const Headers &, const AuthContext &) const;
  Json status(const Headers &, const AuthContext &, uint64_t now) const;
  Response authorize_locked(const std::string &, const std::string &,
                            const Headers &, const AuthContext &) const;
};
} // namespace labbridge
