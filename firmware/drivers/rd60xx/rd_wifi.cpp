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
bool IdentityHeartbeat::expired(uint64_t now) const {
  return failed_ || (waiting_ && now >= deadline_);
}
Bytes IdentityHeartbeat::poll(uint64_t now) {
  if (expired(now) || waiting_ || now < due_)
    return {};
  waiting_ = true;
  deadline_ = now + 2000;
  return read(1, 0, 20);
}
bool IdentityHeartbeat::accept(const Identity &identity, uint64_t now) {
  if (!waiting_ || expired(now) ||
      (identity_ && (identity_->serial != identity.serial ||
                     identity_->model_id != identity.model_id))) {
    failed_ = true;
    return false;
  }
  identity_ = identity;
  waiting_ = false;
  due_ = now + 1000;
  return true;
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
Bytes esptouch_frame(size_t payload_length, std::array<uint8_t, 6> bssid) {
  if (payload_length < 40 || payload_length > 515)
    throw std::invalid_argument("ESPTouch payload length");
  Bytes frame(60 + payload_length, 1);
  std::fill(frame.begin(), frame.begin() + 60, 0);
  frame[0] = 0x08; // Data, non-QoS
  frame[1] = 0x02; // FromDS
  std::fill(frame.begin() + 4, frame.begin() + 10, 0xff);
  std::copy(bssid.begin(), bssid.end(), frame.begin() + 10);
  std::copy(bssid.begin(), bssid.end(), frame.begin() + 16);
  const Bytes llc{0xaa, 0xaa, 0x03, 0, 0, 0, 0x08, 0};
  std::copy(llc.begin(), llc.end(), frame.begin() + 24);
  auto word = [&](size_t offset, uint16_t value) {
    frame[offset] = value >> 8;
    frame[offset + 1] = value;
  };
  frame[32] = 0x45;
  word(34, 28 + payload_length);
  frame[40] = 64;
  frame[41] = 17;
  frame[44] = 192;
  frame[45] = 168;
  frame[46] = 4;
  frame[47] = 1;
  std::fill(frame.begin() + 48, frame.begin() + 52, 0xff);
  uint32_t sum = 0;
  for (size_t i = 32; i < 52; i += 2)
    sum += (uint16_t(frame[i]) << 8) | frame[i + 1];
  while (sum >> 16)
    sum = (sum & 0xffff) + (sum >> 16);
  word(42, uint16_t(~sum));
  word(52, 18266);
  word(54, 7001);
  word(56, 8 + payload_length);
  return frame;
}
std::vector<uint16_t> pc_touch_data(bool server, const std::string &ssid,
                                    const std::string &password,
                                    std::array<uint8_t, 4> ip,
                                    const std::string &profile) {
  if (profile != "pc_v22" && profile != "pc_v22_target_ssid")
    throw std::invalid_argument("Unknown PC provisioning profile");
  std::string s = server && profile == "pc_v22" ? "RD6006" : ssid, p = password;
  if (server) {
    p.clear();
    for (auto octet : ip) {
      if (!p.empty())
        p += '.';
      p += std::to_string(octet);
    }
  }
  auto ascii = [](const std::string &v) {
    return std::all_of(v.begin(), v.end(),
                       [](unsigned char c) { return c >= 32 && c <= 126; });
  };
  if (s.empty() || s.size() > 32 || p.size() > 64 || !ascii(s) || !ascii(p) ||
      9 + s.size() + p.size() > 128)
    throw std::invalid_argument("PC ESPTouch ASCII field length");
  std::vector<uint8_t> bytes{uint8_t(9 + s.size() + p.size()),
                             uint8_t(p.size()), touch_crc({s.begin(), s.end()}),
                             199, 0};
  bytes.insert(bytes.end(), ip.begin(), ip.end());
  bytes.insert(bytes.end(), p.begin(), p.end());
  bytes.insert(bytes.end(), s.begin(), s.end());
  uint8_t checksum = 0;
  for (auto value : bytes)
    checksum ^= value;
  bytes[4] = checksum;
  std::vector<uint16_t> result;
  for (size_t i = 0; i < bytes.size(); ++i) {
    auto value = bytes[i], c = touch_crc({value, uint8_t(i)});
    result.push_back(40 + (c & 0xf0) + (value >> 4));
    result.push_back(296 + i);
    result.push_back(40 + ((c & 15) << 4) + (value & 15));
  }
  return result;
}
void PcTouchSender::start(std::vector<uint16_t> data, uint64_t now, bool wifi) {
  if (data.empty() || data.size() % 3)
    throw std::invalid_argument("Invalid PC touch data");
  data_ = std::move(data);
  cursor_ = 0;
  ready_ = now + (wifi ? 300 : 0);
  deadline_ = ready_ + 60000;
  active_ = true;
}
std::optional<TouchDatagram> PcTouchSender::peek(uint64_t now) const {
  if (!active_ || now < ready_ || (now >= deadline_ && cursor_ == 0))
    return std::nullopt;
  bool next_group = cursor_ < 156 ? cursor_ % 4 == 0 : (cursor_ - 156) % 3 == 0;
  return TouchDatagram{cursor_ < 156 ? uint16_t(515 - cursor_ % 4)
                                     : data_[cursor_ - 156],
                       next_group ? uint8_t(group_ % 100 + 1) : group_};
}
void PcTouchSender::sent(uint64_t now) {
  if (!active_)
    throw std::logic_error("PC touch sender stopped");
  bool next_group = cursor_ < 156 ? cursor_ % 4 == 0 : (cursor_ - 156) % 3 == 0;
  if (next_group)
    group_ = group_ % 100 + 1;
  if (++cursor_ == 156 + data_.size()) {
    cursor_ = 0;
    ready_ = now + 1000;
  }
}
std::optional<std::array<uint8_t, 4>> pc_join_address(const Bytes &bytes) {
  if (bytes.size() != 11 || bytes[7] == 0 || bytes[7] >= 224)
    return std::nullopt;
  return std::array<uint8_t, 4>{bytes[7], bytes[8], bytes[9], bytes[10]};
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
