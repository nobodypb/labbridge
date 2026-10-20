#pragma once
#include "labbridge/node.hpp"
#include "rd60xx.hpp"
#include <map>
#include <optional>
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
// Read-only discovery liveness. Configured instruments use their own driver
// polling/recovery; unregistered sockets must not pin serial claims forever.
class IdentityHeartbeat {
  std::optional<Identity> identity_;
  uint64_t due_ = 0, deadline_ = 0;
  bool waiting_ = false, failed_ = false;

public:
  Bytes poll(uint64_t now);
  bool accept(const Identity &, uint64_t now);
  bool expired(uint64_t now) const;
};
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
// Open AP FromDS/LLC/IPv4/UDP broadcast, without radio FCS. The Wi-Fi driver
// supplies the sequence number/FCS. Only ESPTouch's bounded lengths are
// accepted.
Bytes esptouch_frame(size_t payload_length, std::array<uint8_t, 6> bssid);
// RIDEN PC v1.0.0.22 dialect, independently specified in docs/handbook.md.
std::vector<uint16_t> pc_touch_data(bool server_address,
                                    const std::string &ssid,
                                    const std::string &password,
                                    std::array<uint8_t, 4> ip,
                                    const std::string &profile = "pc_v22");
struct TouchDatagram {
  uint16_t length;
  uint8_t group; // IPv4 destination 234.group.group.group, UDP port 7001.
};
class PcTouchSender {
  std::vector<uint16_t> data_;
  size_t cursor_ = 0;
  uint8_t group_ = 0;
  uint64_t ready_ = 0, deadline_ = 0;
  bool active_ = false;

public:
  void start(std::vector<uint16_t>, uint64_t now, bool wifi_phase);
  void stop() { active_ = false; }
  std::optional<TouchDatagram> peek(uint64_t now) const;
  // Commit only an accepted datagram. Backpressure cannot skip data/groups.
  void sent(uint64_t now);
};
std::optional<std::array<uint8_t, 4>> pc_join_address(const Bytes &);
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
  std::string session_id() const { return id_; }
  Json status(uint64_t now) const;
};
} // namespace labbridge::rd60xx
