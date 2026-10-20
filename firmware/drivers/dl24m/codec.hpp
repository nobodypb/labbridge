#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
namespace labbridge::dl24m {
using Bytes = std::vector<uint8_t>;
struct Reading {
  double voltage = 0, current = 0, power = 0, capacity_ah = 0, energy_wh = 0,
         temperature = 0;
};
struct LoadRegister {
  const char *name;
  uint8_t command;
};
extern const std::array<LoadRegister, 22> dl24m_reads;
uint16_t be16(const uint8_t *);
uint32_t be24(const uint8_t *);
uint32_t be32(const uint8_t *);
bool dl24m_read_only(uint8_t command);
bool atorch_report(const Bytes &, bool ignore_checksum, Reading &);
Bytes px3_command(uint8_t command, uint32_t value = 0);
bool px3_reply(const Bytes &, uint8_t command, uint32_t &value);
class AtorchStream {
  std::vector<uint8_t> buffer_;

public:
  std::vector<Bytes> feed(const uint8_t *, size_t);
  void clear() { buffer_.clear(); }
};
} // namespace labbridge::dl24m
