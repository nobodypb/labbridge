#pragma once
#include "node.hpp"
namespace labbridge {
Json parse_stream_message(const std::string &);
using StreamSend = std::function<int(const char *, size_t)>;
bool write_stream_bytes(const char *, size_t, const StreamSend &);
// Transport-independent, boot-local USB-envelope RPC. A snapshot has one slot:
// replacing an unsent snapshot is explicit loss, never an unbounded send queue.
class StreamSession {
public:
  using Verify = std::function<bool(const std::string &)>;
  using Dispatch = std::function<Response(
      const std::string &, const std::string &, const Json &, const Headers &)>;
  StreamSession(std::string boot, Verify verify, Dispatch dispatch)
      : boot_(std::move(boot)), verify_(std::move(verify)),
        dispatch_(std::move(dispatch)) {}
  Json receive(const Json &, uint64_t);
  static uint64_t status_fingerprint(const Json &);
  void mark_gap() { gap_ = true; }
  bool closing_required() const { return overloaded_; }
  bool authenticated() const { return !token_.empty() && verify_(token_); }
  bool authentication_expired(uint64_t now) const {
    return !authenticated() && now > 3000;
  }
  void offer_snapshot(Json);
  Json take_snapshot();
  std::string take_encoded_snapshot();
  void discard_snapshot();
  void prepare_request(const Json &);
  bool has_snapshot() const { return pending_; }
  const std::string &device() const { return device_; }
  unsigned interval_ms() const { return interval_; }
  const std::vector<std::string> &operations() const { return operations_; }

private:
  Json reply(const Json &, Response) const;
  std::string boot_, token_, device_;
  Verify verify_;
  Dispatch dispatch_;
  unsigned interval_ = 1000;
  uint64_t sequence_ = 0;
  bool pending_ = false, gap_ = false, auth_failed_ = false,
       overloaded_ = false;
  std::string snapshot_;
  std::vector<std::string> operations_, completed_;
};
} // namespace labbridge
