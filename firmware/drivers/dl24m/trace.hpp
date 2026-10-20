#pragma once
#include "labbridge/node.hpp"
#include "labbridge/transaction.hpp"
#include <deque>
namespace labbridge::dl24m {
// Observe the exact endpoint traffic without consuming bytes, issuing commands,
// or changing the authoritative transaction parser and recovery behavior.
class TraceEndpoint final : public Endpoint {
public:
  explicit TraceEndpoint(Endpoint &wire) : wire_(wire) {}
  void clock(uint64_t now) { now_ = now; }
  bool connected() const override { return wire_.connected(); }
  uint64_t generation() const override { return wire_.generation(); }
  bool writable() const override { return wire_.writable(); }
  void cancel_pending() override { wire_.cancel_pending(); }
  void reset_connection() override { wire_.reset_connection(); }
  bool send(const Bytes &bytes) override {
    bool accepted = wire_.send(bytes);
    if (accepted)
      record(bytes, true);
    return accepted;
  }
  Bytes receive() override {
    auto bytes = wire_.receive();
    record(bytes, false);
    return bytes;
  }
  Json diagnostics() const {
    auto list = Json::array();
    constexpr char digits[] = "0123456789abcdef";
    auto native = wire_.trace_supported();
    auto observations = wire_.traffic();
    if (!native)
      for (const auto &entry : entries_)
        observations.push_back({entry.sequence, entry.now, entry.generation,
                                entry.tx, entry.truncated, entry.bytes});
    for (const auto &entry : observations) {
      std::string hex;
      for (auto byte : entry.bytes) {
        hex += digits[byte >> 4];
        hex += digits[byte & 15];
      }
      list.append(Json()
                      .set("sequence", double(entry.sequence))
                      .set("received_ms", double(entry.received_ms))
                      .set("generation", double(entry.generation))
                      .set("direction", entry.transmitted ? "tx" : "rx")
                      .set("origin", native ? "ble-stack" : "endpoint")
                      .set("hex", hex)
                      .set("truncated", entry.truncated));
    }
    Json result;
    result.set("entries", std::move(list))
        .set("dropped",
             double(native ? (observations.empty()
                                  ? 0
                                  : observations.front().sequence - 1)
                           : dropped_));
    return result;
  }

private:
  struct Entry {
    uint64_t sequence, now, generation;
    bool tx, truncated;
    Bytes bytes;
  };
  Endpoint &wire_;
  uint64_t now_ = 0, sequence_ = 0, dropped_ = 0;
  std::deque<Entry> entries_;
  void record(const Bytes &bytes, bool tx) {
    if (bytes.empty() || wire_.trace_supported())
      return;
    if (entries_.size() == 32) {
      entries_.pop_front();
      ++dropped_;
    }
    const auto n = std::min(size_t(64), bytes.size());
    entries_.push_back({++sequence_, now_, generation(), tx, bytes.size() > n,
                        Bytes(bytes.begin(), bytes.begin() + n)});
  }
};
} // namespace labbridge::dl24m
