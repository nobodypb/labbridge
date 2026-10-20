#include "protocol.hpp"
#include <algorithm>
namespace lab {
const std::array<LoadRegister, 22> dl24m_reads{{
    {"output", 0x10}, {"voltage", 0x11}, {"current", 0x12},
    {"elapsed_time", 0x13}, {"capacity", 0x14}, {"energy", 0x15},
    {"temperature", 0x16}, {"set_current", 0x17},
    {"cutoff_voltage", 0x18}, {"discharge_time", 0x19}, {"mode", 0x21},
    {"setting_32", 0x32}, {"setting_33", 0x33}, {"setting_34", 0x34},
    {"setting_35", 0x35}, {"setting_36", 0x36}, {"setting_37", 0x37},
    {"setting_38", 0x38}, {"setting_39", 0x39}, {"setting_3a", 0x3a},
    {"setting_3b", 0x3b}, {"setting_3c", 0x3c},
}};
bool dl24m_read_only(uint8_t command) {
  return std::any_of(dl24m_reads.begin(), dl24m_reads.end(),
                    [command](const auto &r) { return r.command == command; });
}
uint16_t be16(const uint8_t *p) { return (uint16_t(p[0]) << 8) | p[1]; }
uint32_t be24(const uint8_t *p) { return (uint32_t(p[0]) << 16) | be16(p + 1); }
uint32_t be32(const uint8_t *p) {
  return (uint32_t(be16(p)) << 16) | be16(p + 2);
}
uint16_t crc16(const uint8_t *p, size_t n) {
  uint16_t c = 0xffff;
  while (n--) {
    c ^= *p++;
    for (int b = 0; b < 8; b++)
      c = (c >> 1) ^ ((c & 1) ? 0xa001 : 0);
  }
  return c;
}
static Bytes modbus(uint8_t fn, uint16_t a, uint16_t b) {
  Bytes p{1, fn, uint8_t(a >> 8), uint8_t(a), uint8_t(b >> 8), uint8_t(b)};
  auto c = crc16(p.data(), p.size());
  p.push_back(c & 255);
  p.push_back(c >> 8);
  return p;
}
Bytes modbus_read(uint16_t a, uint16_t n) { return modbus(3, a, n); }
Bytes modbus_write(uint16_t a, uint16_t v) { return modbus(6, a, v); }
bool modbus_reply(const Bytes &p, uint8_t fn, size_t bytes) {
  if (p.size() != (fn == 3 ? bytes + 5 : 8) || p[0] != 1 || p[1] != fn)
    return false;
  if (fn == 3 && p[2] != bytes)
    return false;
  return crc16(p.data(), p.size()) == 0;
}
bool atorch_report(const Bytes &p, bool ignore, Reading &r) {
  if (p.size() != 36 || p[0] != 255 || p[1] != 85 || p[2] != 1 || p[3] != 2)
    return false;
  uint8_t sum = 0;
  for (size_t i = 2; i < 35; i++)
    sum += p[i];
  if (!ignore && uint8_t(sum ^ 0x44) != p[35])
    return false;
  Reading t;
  t.voltage = be24(&p[4]) * 0.1;
  t.current = be24(&p[7]) * 0.001;
  t.power = t.voltage * t.current;
  t.capacity_ah = be24(&p[10]) * 0.01;
  t.energy_wh = be32(&p[13]) * 10.0;
  t.temperature = be16(&p[24]);
  if (t.voltage > 210 || t.current > 45 || t.temperature > 150 || p[28] > 59 ||
      p[29] > 59)
    return false;
  r = t;
  return true;
}
Bytes px_command(uint8_t cmd, uint8_t a, uint8_t b) {
  return {0xb1, 0xb2, cmd, a, b, 0xb6};
}
Bytes px_current(double a) {
  if (!std::isfinite(a) || a < 0 || a > 40)
    return {};
  auto c = unsigned(std::llround(a * 100));
  return px_command(2, c / 100, c % 100);
}
Bytes px3_command(uint8_t cmd, uint32_t value) {
  if (value > 0xffffff)
    return {};
  return {0xb1,
          0xb2,
          cmd,
          uint8_t(value >> 16),
          uint8_t(value >> 8),
          uint8_t(value),
          0,
          0,
          0xb6};
}
bool px3_reply(const Bytes &frame, uint8_t command, uint32_t &value) {
  if (frame.size() != 10 || frame[0] != 0xca || frame[1] != 0xcb ||
      frame[2] != command || frame[8] != 0xce || frame[9] != 0xcf)
    return false;
  value = be24(&frame[3]);
  return true;
}
Bytes spp_command(uint8_t command, uint32_t value) {
  return px3_command(command, value);
}
bool spp_reply(const Bytes &frame, uint8_t command, uint32_t &value) {
  return px3_reply(frame, command, value);
}
std::vector<Bytes> AtorchStream::feed(const uint8_t *p, size_t n) {
  std::vector<Bytes> frames;
  if (n > 1024) {
    buffer_.clear();
    return frames;
  }
  buffer_.insert(buffer_.end(), p, p + n);
  while (!buffer_.empty()) {
    size_t len = 0;
    if (buffer_[0] == 0x6f)
      len = 1;
    else if (buffer_[0] == 0xff || buffer_[0] == 0xca) {
      if (buffer_.size() < 2)
        break;
      if (buffer_[0] == 0xca && buffer_[1] == 0xcb) {
        if (buffer_.size() >= 7 && buffer_[5] == 0xce &&
            buffer_[6] == 0xcf)
          len = 7;
        else if (buffer_.size() >= 10)
          len = 10;
        else
          break;
      }
      else if (buffer_[0] == 0xff && buffer_[1] == 0x55) {
        if (buffer_.size() < 3)
          break;
        if (buffer_[2] == 1)
          len = 36;
        else if (buffer_[2] == 2)
          len = 8;
      }
    }
    if (!len) {
      buffer_.erase(buffer_.begin());
      continue;
    }
    if (buffer_.size() < len)
      break;
    if (len == 36) {
      // Observed on DL24M V2.0: a numeric reply can interrupt an FF55
      // report. Do not consume that reply as telemetry payload. Preserve
      // valid checksummed reports, even if their payload resembles a reply.
      Reading reading;
      Bytes candidate(buffer_.begin(), buffer_.begin() + len);
      if (!atorch_report(candidate, false, reading)) {
        size_t interrupted = 0;
        bool pending = false;
        for (size_t i = 4; i < len; ++i) {
          if (buffer_[i] != 0xca)
            continue;
          if (buffer_.size() <= i + 1) {
            pending = true;
            break;
          }
          if (buffer_[i + 1] != 0xcb)
            continue;
          if (buffer_.size() < i + 10) {
            pending = true;
            break;
          }
          if ((dl24m_read_only(buffer_[i + 2]) ||
               buffer_[i + 2] == 0x22 || buffer_[i + 2] == 0x31) &&
              buffer_[i + 6] == 0 && buffer_[i + 7] == 0 &&
              buffer_[i + 8] == 0xce && buffer_[i + 9] == 0xcf) {
            interrupted = i;
            break;
          }
        }
        if (pending)
          break;
        if (interrupted) {
          buffer_.erase(buffer_.begin(), buffer_.begin() + interrupted);
          continue;
        }
      }
    }
    if (len == 7 && (buffer_[5] != 0xce || buffer_[6] != 0xcf)) {
      buffer_.erase(buffer_.begin());
      continue;
    }
    if (len == 10 && (buffer_[8] != 0xce || buffer_[9] != 0xcf)) {
      buffer_.erase(buffer_.begin());
      continue;
    }
    frames.emplace_back(buffer_.begin(), buffer_.begin() + len);
    buffer_.erase(buffer_.begin(), buffer_.begin() + len);
  }
  return frames;
}
bool Limits::valid() const {
  return std::isfinite(voltage) && std::isfinite(current) &&
         std::isfinite(power) && voltage > 0 && current > 0 && power > 0;
}
bool Limits::contains(double v, double i) const {
  return valid() && std::isfinite(v) && std::isfinite(i) && v >= 0 && i >= 0 &&
         v <= voltage && i <= current && v * i <= power + 1e-6;
}
} // namespace lab
