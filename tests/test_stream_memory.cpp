#include "dl24m_driver.hpp"
#include "labbridge/stream.hpp"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
using namespace labbridge;
struct alignas(std::max_align_t) Allocation {
  size_t size;
};
static size_t used = 0, peak = 0;
static void *allocate(size_t n) {
  auto p = static_cast<Allocation *>(std::malloc(n + sizeof(Allocation)));
  if (!p)
    return nullptr;
  p->size = n;
  used += n;
  peak = std::max(peak, used);
  return p + 1;
}
static void release(void *v) {
  if (!v)
    return;
  auto p = static_cast<Allocation *>(v) - 1;
  used -= p->size;
  std::free(p);
}
struct Wire : Endpoint {
  Bytes rx;
  bool connected() const override { return true; }
  uint64_t generation() const override { return 1; }
  bool send(const Bytes &bytes) override {
    auto c = bytes[2];
    if (c >= 0x10) {
      auto value = c == 0x11 ? 24000 : c == 0x17 ? 100 : c == 0x16 ? 250 : 0;
      rx = {0xca,
            0xcb,
            c,
            uint8_t(value >> 16),
            uint8_t(value >> 8),
            uint8_t(value),
            0,
            0,
            0xce,
            0xcf};
    }
    return true;
  }
  Bytes receive() override {
    Bytes b;
    b.swap(rx);
    return b;
  }
};
int main() {
  cJSON_Hooks hooks{allocate, release};
  cJSON_InitHooks(&hooks);
  // Validation must not retain the complete API schema in scarce ESP heap.
  {
    auto body = Json::parse(R"({"mode":"cv","value":23.9,"unit":"V"})");
    auto baseline = used;
    std::string reason;
    assert(validate_request("setDl24mTarget", body, reason));
    assert(used == baseline);
  }
  Wire wire;
  Node node("test", "boot", "sim");
  node.add("load", std::make_shared<Dl24mDriver>(
                       "load", "synthetic", wire, Limits{25, .2, 5, 3000, 2000},
                       [](const Json &) {}, Json()));
  for (unsigned t = 0; t < 5000; t += 5)
    node.tick(t);
  peak = used;
  {
    auto response = node.request("GET", "/api/v1/devices", Json(), {}, 5000);
    fprintf(stderr, "After request: used %zu peak %zu\n", used, peak);
    Json reply;
    reply.set("id", "request")
        .set("status", 200.)
        .set("body", std::move(response.body))
        .set("headers", Json());
    fprintf(stderr, "After envelope: used %zu peak %zu\n", used, peak);
    auto output = reply.dump();
    fprintf(stderr, "USB discovery: JSON peak %zu bytes, message %zu bytes\n",
            peak, output.size());
    assert(peak <= 32768);
  }
  peak = used;
  {
    StreamSession session(
        "boot", [](const std::string &) { return true; },
        [](const std::string &, const std::string &, const Json &,
           const Headers &) { return Response{200, Json()}; });
    session.receive(Json::parse(R"({"type":"auth","token":"secret"})"), 0);
    session.offer_snapshot(node.stream_snapshot("load", {}, 5000));
    auto event = session.take_snapshot();
    auto output = event.dump();
    fprintf(stderr, "Stream snapshot: JSON peak %zu bytes, message %zu bytes\n",
            peak, output.size());
    assert(peak <= 49152);
  }

  peak = used;
  {
    StreamSession session(
        "boot", [](const std::string &) { return true; },
        [&](const std::string &method, const std::string &path,
            const Json &body, const Headers &headers) {
          return node.request(method, path, body, headers, 5000);
        });
    session.receive(Json::parse(R"({"type":"auth","token":"secret"})"), 0);
    auto reply = session.receive(
        Json::parse(
            R"({"id":"capture","method":"GET","path":"/api/v1/devices/load/dl24m/diagnostics"})"),
        1);
    auto output = reply.dump();
    fprintf(stderr, "Capture reply: JSON peak %zu bytes, message %zu bytes\n",
            peak, output.size());
    assert(peak <= 65536 &&
           cJSON_GetArraySize(reply.item("body").at("entries")) <= 32);
  }
  auto lease = node.request(
      "POST", "/api/v1/leases",
      Json::parse(R"({"device_ids":["load"],"ttl_ms":1000})"), {}, 5000);
  assert(lease.status == 200);
  peak = used;
  auto baseline = used;
  node.tick(5005);
  assert(peak == baseline); // Lease supervision must not rebuild UI metadata.
}
