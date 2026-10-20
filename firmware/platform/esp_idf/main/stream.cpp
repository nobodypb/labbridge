#include "labbridge/stream.hpp"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "platform.hpp"
#include <cerrno>
#include <cstdio>
#include <memory>
namespace labbridge::esp {
namespace {
// Only HTTPD's own task writes frames. The worker only copies cached node data;
// no socket I/O occurs under the node's safety mutex or on its tick task.
struct Peer {
  int fd;
  uint64_t opened, next_sample = 0, next_status = 0, next_catalogue = 0;
  bool queued = false, closing = false, replying = false;
  uint64_t last_status = 0, max_build_ms = 0;
  StreamSession session;
  Peer(int socket, std::string boot)
      : fd(socket), opened(now_ms()),
        session(
            std::move(boot),
            [](const std::string &token) {
              std::lock_guard<std::recursive_mutex> lock(mutex);
              return !token.empty() && token == read_nvs("auth").text("token");
            },
            [](const std::string &method, const std::string &path,
               const Json &body, const Headers &headers) {
              return dispatch(method, path, body, headers, false);
            }) {}
};
std::mutex stream_mutex;
std::shared_ptr<Peer> peer;
httpd_handle_t server;
TaskHandle_t worker_task;
void failed(const char *stage) {
#ifdef CONFIG_LAB_SERIAL_DIAGNOSTICS
  printf("WebSocket failure at %s: free=%u largest=%u bytes\n", stage,
         unsigned(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
         unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
#endif
}
void freed(void *context) {
  uint64_t max_build_ms = 0;
  {
    std::lock_guard<std::mutex> lock(stream_mutex);
    if (peer.get() == context) {
      max_build_ms = peer->max_build_ms;
      peer.reset();
    }
  }
#ifdef CONFIG_LAB_SERIAL_DIAGNOSTICS
  printf("WebSocket released: free=%u min_free=%u worker_stack=%u bytes "
         "build_max=%u ms\n",
         unsigned(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
         unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT)),
         unsigned(uxTaskGetStackHighWaterMark(worker_task)),
         unsigned(max_build_ms));
#endif
}
// IDF's WS sender accepts partial positive writes from send_fn. Complete each
// header/payload write ourselves, or close on deadline rather than corrupting
// the next frame. This override is installed only on our plain HTTP WS socket.
int send_complete(httpd_handle_t, int fd, const char *data, size_t size,
                  int flags) {
  auto deadline = now_ms() + 1000;
  bool ok =
      write_stream_bytes(data, size, [&](const char *bytes, size_t count) {
        for (;;) {
          auto now = now_ms();
          if (now >= deadline)
            return -1;
          auto sent = ::send(fd, bytes, count, flags | MSG_DONTWAIT);
          if (sent >= 0)
            return int(sent);
          if (errno == EINTR)
            continue;
          if (errno != EAGAIN && errno != EWOULDBLOCK)
            return -1;
          fd_set writable;
          FD_ZERO(&writable);
          FD_SET(fd, &writable);
          auto remaining = deadline - now;
          timeval timeout{long(remaining / 1000),
                          long(remaining % 1000) * 1000};
          auto ready = select(fd + 1, nullptr, &writable, nullptr, &timeout);
          if (ready == 0 || (ready < 0 && errno != EINTR))
            return -1;
        }
      });
  return ok ? int(size) : -1;
}
esp_err_t send_text(int fd, const std::string &text) {
  if (text.size() > 32768)
    return ESP_ERR_INVALID_SIZE;
  httpd_ws_frame_t frame{};
  frame.type = HTTPD_WS_TYPE_TEXT;
  frame.payload = reinterpret_cast<uint8_t *>(const_cast<char *>(text.data()));
  frame.len = text.size();
  return httpd_ws_send_frame_async(server, fd, &frame);
}
esp_err_t send(int fd, const Json &message) {
  try {
    return send_text(fd, message.dump());
  } catch (const std::exception &) {
    failed("reply encoding");
    return ESP_ERR_NO_MEM;
  }
}
void send_snapshot(void *context) try {
  std::unique_ptr<std::shared_ptr<Peer>> holder(
      static_cast<std::shared_ptr<Peer> *>(context));
  auto current = *holder;
  std::string message;
  {
    std::lock_guard<std::mutex> lock(stream_mutex);
    if (peer != current || current->closing ||
        !current->session.authenticated()) {
      current->queued = false;
      return;
    }
    // A waiting RPC (including STOP and explicit heartbeat) takes precedence.
    uint8_t byte;
    if (recv(current->fd, &byte, 1, MSG_PEEK | MSG_DONTWAIT) > 0 ||
        !current->session.has_snapshot()) {
      current->queued = false;
      return;
    }
    message = current->session.take_encoded_snapshot();
  }
  auto ok =
      httpd_ws_get_fd_info(server, current->fd) == HTTPD_WS_CLIENT_WEBSOCKET &&
      send_text(current->fd, message) == ESP_OK;
  {
    std::lock_guard<std::mutex> lock(stream_mutex);
    current->queued = false;
    if (!ok)
      current->closing = true;
  }
  if (!ok)
    httpd_sess_trigger_close(server, current->fd);
} catch (const std::exception &) {
  failed("snapshot send");
  // Allocation or token-store failures close this session without escaping
  // HTTPD.
  std::lock_guard<std::mutex> lock(stream_mutex);
  if (peer) {
    peer->closing = true;
    httpd_sess_trigger_close(server, peer->fd);
  }
}
esp_err_t websocket(httpd_req_t *request) try {
  auto fd = httpd_req_to_sockfd(request);
  if (request->method == HTTP_GET) {
    std::lock_guard<std::mutex> lock(stream_mutex);
    if (peer || board.capacity.at("websocket") == 0)
      return ESP_FAIL;
    if (httpd_sess_set_send_override(server, fd, send_complete) != ESP_OK)
      return ESP_FAIL;
    std::string boot;
    {
      std::lock_guard<std::recursive_mutex> node_lock(mutex);
      boot = node->request("GET", "/api/v1/node", Json(), {}, now_ms())
                 .body.text("boot_id");
    }
    peer = std::make_shared<Peer>(fd, boot);
    request->sess_ctx = peer.get();
    request->free_ctx = freed;
    return ESP_OK;
  }
  std::shared_ptr<Peer> current;
  {
    std::lock_guard<std::mutex> lock(stream_mutex);
    current = peer;
    if (!current || current->fd != fd)
      return ESP_FAIL;
  }
  httpd_ws_frame_t frame{};
  if (httpd_ws_recv_frame(request, &frame, 0) != ESP_OK)
    return ESP_FAIL;
  if (frame.type == HTTPD_WS_TYPE_CLOSE)
    return ESP_FAIL;
  // Ping/pong are handled by HTTPD. They never enter lease execution.
  if (frame.type != HTTPD_WS_TYPE_TEXT || frame.fragmented ||
      frame.len > 16384 || frame.len == 0)
    return ESP_FAIL;
  std::string input(frame.len, '\0');
  frame.payload = reinterpret_cast<uint8_t *>(input.data());
  if (httpd_ws_recv_frame(request, &frame, frame.len) != ESP_OK)
    return ESP_FAIL;
  try {
    Json response;
    bool authenticated, close_after_response;
    {
      std::lock_guard<std::mutex> lock(stream_mutex);
      current->replying = true;
      auto message = parse_stream_message(input);
      current->session.prepare_request(message);
      response = current->session.receive(message, now_ms() - current->opened);
      authenticated = current->session.authenticated();
      close_after_response = current->session.closing_required();
      if (message.text("type") == "subscribe") {
        current->next_sample = 0;
        current->next_catalogue = 0;
      }
      current->next_status = 0;
    }
    auto result = send(fd, response);
    {
      std::lock_guard<std::mutex> lock(stream_mutex);
      current->replying = false;
    }
    return authenticated && !close_after_response ? result : ESP_FAIL;
  } catch (const std::exception &) {
    std::lock_guard<std::mutex> lock(stream_mutex);
    failed("RPC");
    current->replying = false;
    current->closing = true;
    return ESP_FAIL;
  }
} catch (const std::exception &) {
  failed("frame");
  return ESP_FAIL;
}
void worker(void *) {
  for (;;) {
    std::shared_ptr<Peer> current;
    try {
      std::lock_guard<std::mutex> lock(stream_mutex);
      current = peer;
      auto now = now_ms();
      if (current && !current->closing) {
        if (current->session.closing_required() ||
            (!current->session.authenticated() &&
             now - current->opened > 3000) ||
            httpd_ws_get_fd_info(server, current->fd) !=
                HTTPD_WS_CLIENT_WEBSOCKET) {
          current->closing = true;
          httpd_sess_trigger_close(server, current->fd);
        } else if (current->session.authenticated() && !current->replying &&
                   !current->queued) {
          // Status/operation confirmations at 100 ms; measurements at requested
          // rate. Catalogue data is refreshed separately; subscription starts
          // with a complete snapshot, so reconnect needs no replay.
          try {
            if (now >= current->next_status) {
              // Keep the bounded JSON construction/encoding mutually exclusive
              // with journal persistence. Transient telemetry allocations must
              // not exhaust RAM underneath a safety operation. Socket I/O still
              // runs later on HTTPD, entirely outside this mutex.
              std::lock_guard<std::recursive_mutex> node_lock(mutex);
              auto build_started = now_ms();
              now = build_started; // Freshness uses the time after taking the
                                   // node lock.
              bool sample = now >= current->next_sample;
              bool catalogue = now >= current->next_catalogue;
              auto snapshot = node->stream_snapshot(
                  current->session.device(), current->session.operations(), now,
                  catalogue, sample);
              if (sample)
                current->next_sample = now + current->session.interval_ms();
              if (catalogue)
                current->next_catalogue = now + 5000;
              current->next_status = now + 100;
              auto signature = StreamSession::status_fingerprint(snapshot);
              if (sample || catalogue || signature != current->last_status) {
                current->last_status = signature;
                current->session.offer_snapshot(std::move(snapshot));
              }
              current->max_build_ms =
                  std::max(current->max_build_ms, now_ms() - build_started);
            }
            if (current->session.has_snapshot() && !current->queued) {
              auto context = new std::shared_ptr<Peer>(current);
              current->queued = true;
              if (httpd_queue_work(server, send_snapshot, context) != ESP_OK) {
                current->queued = false;
                delete context;
                current->closing = true;
                httpd_sess_trigger_close(server, current->fd);
              }
            }
          } catch (const std::exception &) {
            failed("snapshot encoding");
            current->closing = true;
            httpd_sess_trigger_close(server, current->fd);
          }
        } else if (current->queued &&
                   now >=
                       current->next_sample + current->session.interval_ms()) {
          // A blocked sender does not accumulate more JSON trees/encoded
          // frames. Report skipped measurement intervals if this connection
          // recovers.
          current->session.mark_gap();
        }
      }
    } catch (const std::exception &) {
      failed("worker");
      if (current) {
        std::lock_guard<std::mutex> lock(stream_mutex);
        current->closing = true;
        httpd_sess_trigger_close(server, current->fd);
      }
    }
    sleep_ms(25);
  }
}
} // namespace
void start_stream(httpd_handle_t handle) {
  server = handle;
  httpd_uri_t route{};
  route.uri = "/api/v1/ws";
  route.method = HTTP_GET;
  route.handler = websocket;
  route.is_websocket = true;
  ESP_ERROR_CHECK(httpd_register_uri_handler(server, &route));
  xTaskCreate(worker, "api_stream", 8192, nullptr, 2, &worker_task);
#ifdef CONFIG_LAB_SERIAL_DIAGNOSTICS
  printf("WebSocket budget: free=%u largest=%u bytes\n",
         unsigned(heap_caps_get_free_size(MALLOC_CAP_8BIT)),
         unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
#endif
}
} // namespace labbridge::esp
