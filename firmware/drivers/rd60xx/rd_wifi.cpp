#include "rd_wifi.hpp"
#include <algorithm>
namespace labbridge::rd60xx {
std::vector<Bytes> RtuStream::feed(const Bytes &input) {
  std::vector<Bytes> frames;
  for (auto byte : input) {
    buffer_.push_back(byte);
    if (buffer_.size() > 255)
      buffer_.erase(buffer_.begin());
    // A corrupt, plausible length must not hide a complete later frame.
    for (size_t offset = 0; offset + 2 <= buffer_.size(); ++offset) {
      const auto *p = buffer_.data() + offset;
      if (p[0] < 1 || p[0] > 247)
        continue;
      size_t size = 0;
      if (p[1] == 6)
        size = 8;
      else if (p[1] == 0x83 || p[1] == 0x86)
        size = 5;
      else if (p[1] == 3 && offset + 3 <= buffer_.size() && p[2] &&
               p[2] <= 250 && !(p[2] % 2))
        size = p[2] + 5;
      if (!size || offset + size > buffer_.size() || crc(p, size))
        continue;
      frames.emplace_back(p, p + size);
      buffer_.erase(buffer_.begin(), buffer_.begin() + offset + size);
      break;
    }
  }
  return frames;
}
Identity identify(const Bytes &frame) {
  if (frame.size() != 45 || !valid(frame, 1, 3))
    throw std::invalid_argument("Invalid RD identity response");
  std::array<uint16_t, 20> r{};
  for (size_t i = 0; i < r.size(); ++i)
    r[i] = (uint16_t(frame[3 + 2 * i]) << 8) | frame[4 + 2 * i];
  auto model = r[0] / 10;
  auto value = decode(r, model);
  if (!value.serial)
    throw std::invalid_argument("RD serial is zero");
  return {value.serial, uint16_t(model), r[0], r[3]};
}
void IdentityBindings::configure(uint32_t serial, unsigned model) {
  profile(model);
  if (!serial || expected_.count(serial))
    throw std::invalid_argument("Duplicate/zero RD serial");
  expected_[serial] = model;
}
IdentityBindings::Claim IdentityBindings::claim(const Identity &identity,
                                                int connection) {
  if (!identity.serial || connection < 0)
    return Claim::rejected;
  auto expected = expected_.find(identity.serial), unused = expected_.end();
  if (expected != unused && expected->second != identity.model)
    return Claim::rejected;
  auto old = seen_.find(identity.serial);
  if ((old != seen_.end() && old->second.connection >= 0) ||
      (old == seen_.end() && seen_.size() >= 8))
    return Claim::rejected;
  seen_.insert_or_assign(identity.serial, Seen{identity, connection});
  return expected == unused ? Claim::discovered : Claim::bound;
}
void IdentityBindings::release(int connection) {
  for (auto &p : seen_)
    if (p.second.connection == connection)
      p.second.connection = -1;
}
Json IdentityBindings::describe() const {
  auto result = Json::array();
  for (auto &[serial, c] : seen_) {
    Json d;
    d.set("serial", std::to_string(serial))
        .set("model", std::string("RD") + std::to_string(c.identity.model))
        .set("model_id", double(c.identity.model_id))
        .set("firmware_version", double(c.identity.firmware))
        .set("connected", c.connection >= 0)
        .set("configured", expected_.count(serial) != 0);
    result.append(d);
  }
  return result;
}
static uint8_t touch_crc(const std::vector<uint8_t> &data) {
  uint8_t c = 0;
  for (auto v : data) {
    c ^= v;
    for (int n = 0; n < 8; ++n)
      c = (c & 1) ? (c >> 1) ^ 0x8c : c >> 1;
  }
  return c;
}
TouchPackets esptouch_lengths(const std::string &ssid,
                              const std::string &password,
                              std::array<uint8_t, 4> ip,
                              std::array<uint8_t, 6> bssid) {
  if (ssid.empty() || ssid.size() > 32 || password.size() > 64 ||
      9 + ssid.size() + password.size() + 6 > 128)
    throw std::invalid_argument("ESPTouch field length");
  std::vector<uint8_t> data{uint8_t(9 + ssid.size() + password.size()),
                            uint8_t(password.size()),
                            touch_crc({ssid.begin(), ssid.end()}),
                            touch_crc({bssid.begin(), bssid.end()}), 0};
  data.insert(data.end(), ip.begin(), ip.end());
  data.insert(data.end(), password.begin(), password.end());
  data.insert(data.end(), ssid.begin(), ssid.end());
  uint8_t checksum = 0;
  for (auto b : data)
    checksum ^= b;
  data[4] = checksum;
  // Interleave the AP BSSID without including it in the advertised payload
  // length.
  std::vector<std::pair<uint8_t, uint8_t>> codes;
  for (size_t i = 0; i < data.size(); ++i)
    codes.emplace_back(i, data[i]);
  for (size_t i = 0; i < bssid.size(); ++i) {
    auto pos = std::min<size_t>(5 + 4 * i, codes.size());
    codes.insert(codes.begin() + pos, {uint8_t(data.size() + i), bssid[i]});
  }
  TouchPackets result{{515, 514, 513, 512}, {}};
  for (auto [index, value] : codes) {
    auto c = touch_crc({value, index});
    result.data.push_back(40 + ((c & 0xf0) | (value >> 4)));
    result.data.push_back(40 + 0x100 + index);
    result.data.push_back(40 + ((c << 4) & 0xf0) + (value & 15));
  }
  return result;
}
bool Provisioning::active() const {
  return state_ == "server_address" || state_ == "wifi" ||
         state_ == "waiting_connection";
}
void Provisioning::start(const std::string &id, uint64_t now) {
  tick(now);
  if (active())
    throw std::invalid_argument("Provisioning already active");
  id_ = id;
  state_ = "server_address";
  deadline_ = now + 120000;
  server_done_ = wifi_done_ = observed_ = false;
}
void Provisioning::advance(const std::string &id, const std::string &phase,
                           uint64_t now) {
  tick(now);
  if (id.empty() || id != id_)
    throw std::invalid_argument("Wrong provisioning session");
  if (phase == "server_address" && server_done_)
    return;
  if (phase == "wifi" && wifi_done_)
    return;
  if (!active() || phase != state_ || phase == "waiting_connection")
    throw std::invalid_argument("Provisioning phase changed");
  if (phase == "server_address") {
    server_done_ = true;
    state_ = "wifi";
  } else {
    wifi_done_ = true;
    state_ = observed_ ? "complete" : "waiting_connection";
  }
  deadline_ = now + 120000;
}
void Provisioning::cancel(const std::string &id) {
  if (id.empty() || id != id_)
    throw std::invalid_argument("Wrong provisioning session");
  if (active())
    state_ = "cancelled";
}
void Provisioning::tick(uint64_t now) {
  if (active() && now >= deadline_)
    state_ = "timed_out";
}
void Provisioning::connected(const Identity &) {
  observed_ = true;
  if (state_ == "waiting_connection")
    state_ = "complete";
}
void Provisioning::fail() {
  if (active())
    state_ = "failed";
}
Json Provisioning::status(uint64_t) const {
  Json j;
  j.set("state", state_);
  if (!id_.empty())
    j.set("session_id", id_).set("deadline_ms", double(deadline_));
  return j;
}
} // namespace labbridge::rd60xx
