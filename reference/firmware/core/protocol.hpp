#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace lab {
using Bytes = std::vector<uint8_t>;
uint16_t crc16(const uint8_t *, size_t);
Bytes modbus_read(uint16_t start, uint16_t count);
Bytes modbus_write(uint16_t reg, uint16_t value);
bool modbus_reply(const Bytes &, uint8_t function, size_t payload_bytes);
uint16_t be16(const uint8_t *);
uint32_t be24(const uint8_t *);
uint32_t be32(const uint8_t *);
struct Reading {
  double voltage = 0, current = 0, power = 0, capacity_ah = 0, energy_wh = 0,
         temperature = 0;
};
bool atorch_report(const Bytes &, bool ignore_checksum, Reading &);
Bytes px_command(uint8_t command, uint8_t a = 0, uint8_t b = 0);
Bytes px_current(double amps);
Bytes px3_command(uint8_t command, uint32_t value = 0);
bool px3_reply(const Bytes &, uint8_t command, uint32_t &value);
// Excludes 0x22 (read-and-clear event) and 0x31 (enters settings).
struct LoadRegister {
  const char *name;
  uint8_t command;
};
extern const std::array<LoadRegister, 22> dl24m_reads;
bool dl24m_read_only(uint8_t command);
Bytes spp_command(uint8_t command, uint32_t value = 0);
bool spp_reply(const Bytes &, uint8_t command, uint32_t &value);
class AtorchStream {
  Bytes buffer_;

public:
  std::vector<Bytes> feed(const uint8_t *, size_t);
  void clear() { buffer_.clear(); }
};
struct Limits {
  double voltage = 0, current = 0, power = 0;
  bool valid() const;
  bool contains(double v, double i) const;
};
struct Lease {
  std::string token;
  std::array<bool, 3> devices{};
  uint64_t deadline = 0;
  bool stopping = false;
  bool require_wifi = true;
  bool active() const { return !token.empty(); }
  bool expired(uint64_t now) const { return active() && now >= deadline; }
  bool owns(const std::string &t, uint64_t now) const {
    return active() && !stopping && !expired(now) && token == t;
  }
  void clear() {
    token.clear();
    devices = {};
    deadline = 0;
    stopping = false;
    require_wifi = true;
  }
};
} // namespace lab
