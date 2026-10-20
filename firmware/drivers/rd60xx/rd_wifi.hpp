#pragma once
#include "labbridge/node.hpp"
#include "rd60xx.hpp"
#include <map>
namespace labbridge::rd60xx {
class RtuStream {
  Bytes buffer_;

public:
  std::vector<Bytes> feed(const Bytes &);
  void clear() { buffer_.clear(); }
  size_t buffered() const { return buffer_.size(); }
};
struct Identity {
  uint32_t serial;
  uint16_t model, model_id, firmware;
};
Identity identify(const Bytes &);
class IdentityBindings {
  struct Seen {
    Identity identity;
    int connection;
  };
  std::map<uint32_t, unsigned> expected_;
  std::map<uint32_t, Seen> seen_;

public:
  enum class Claim { rejected, discovered, bound };
  void configure(uint32_t serial, unsigned model);
  Claim claim(const Identity &, int connection);
  void release(int connection);
  Json describe() const;
};
struct TouchPackets {
  std::vector<uint16_t> guide, data;
};
TouchPackets esptouch_lengths(const std::string &ssid,
                              const std::string &password,
                              std::array<uint8_t, 4> ip,
                              std::array<uint8_t, 6> bssid);
class Provisioning {
  std::string id_, state_ = "idle";
  uint64_t deadline_ = 0;
  bool server_done_ = false, wifi_done_ = false, observed_ = false;

public:
  bool active() const;
  void start(const std::string &id, uint64_t now);
  void advance(const std::string &id, const std::string &phase, uint64_t now);
  void cancel(const std::string &id);
  void tick(uint64_t now);
  void connected(const Identity &);
  void fail();
  std::string phase() const { return state_; }
  Json status(uint64_t now) const;
};
} // namespace labbridge::rd60xx
