#include "auth_platform.hpp"
#include "esp_random.h"
#include "freertos/queue.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/platform_util.h"
#include "nvs.h"
#include "platform.hpp"
#include <array>
#include <atomic>

namespace labbridge::esp {
namespace {
std::unique_ptr<Auth> auth;
QueueHandle_t jobs;
std::atomic<bool> occupied{false};
struct Job {
  std::string method, path;
  Json body;
  Headers headers;
  AuthContext context;
  httpd_req_t *http = nullptr;
  std::string usb_id;
  Response result{503, Json()};
};
std::string encode(const uint8_t *bytes, size_t n) {
  const char *digits = "0123456789abcdef";
  std::string result;
  result.reserve(n * 2);
  for (size_t i = 0; i < n; ++i) {
    result += digits[bytes[i] >> 4];
    result += digits[bytes[i] & 15];
  }
  return result;
}
std::string random_secret() {
  std::array<uint8_t, 32> bytes;
  // start_auth runs after Wi-Fi startup, which supplies hardware RNG entropy.
  esp_fill_random(bytes.data(), bytes.size());
  return encode(bytes.data(), bytes.size());
}
std::string derive(const std::string &password, const std::string &salt,
                   unsigned rounds) {
  std::array<uint8_t, 32> salt_bytes{}, output{};
  if (salt.size() != 64)
    throw std::invalid_argument("Invalid salt");
  auto digit = [](char c) -> unsigned {
    if (c >= '0' && c <= '9')
      return c - '0';
    if (c >= 'a' && c <= 'f')
      return c - 'a' + 10;
    throw std::invalid_argument("Invalid salt");
  };
  for (size_t i = 0; i < salt_bytes.size(); ++i)
    salt_bytes[i] = uint8_t(digit(salt[i * 2]) * 16 + digit(salt[i * 2 + 1]));
  auto error = mbedtls_pkcs5_pbkdf2_hmac_ext(
      MBEDTLS_MD_SHA256,
      reinterpret_cast<const unsigned char *>(password.data()), password.size(),
      salt_bytes.data(), salt_bytes.size(), rounds, output.size(),
      output.data());
  if (error != 0)
    throw std::runtime_error("Password KDF failed");
  auto result = encode(output.data(), output.size());
  mbedtls_platform_zeroize(output.data(), output.size());
  return result;
}
Response busy() {
  auto response = problem(429, "Password worker busy; try again later");
  response.headers["Retry-After"] = "1";
  return response;
}
void persist_password(const Json &value) {
  auto encoded = value.dump();
  nvs_handle_t handle;
  if (nvs_open("labbridge2", NVS_READWRITE, &handle) != ESP_OK)
    throw std::runtime_error("Password storage unavailable");
  auto error = nvs_set_str(handle, "ui_auth", encoded.c_str());
  if (error == ESP_OK)
    error = nvs_commit(handle);
  nvs_close(handle);
  if (error != ESP_OK)
    throw std::runtime_error("Password storage unavailable");
}
void worker(void *) {
  for (;;) {
    Job *job = nullptr;
    if (xQueueReceive(jobs, &job, portMAX_DELAY) != pdTRUE)
      continue;
    try {
      job->result = auth->request(job->method, job->path, job->body,
                                  job->headers, job->context);
    } catch (...) {
      job->result = problem(503, "Authentication temporarily unavailable");
    }
    if (job->http) {
      http_respond(job->http, std::move(job->result));
      httpd_req_async_handler_complete(job->http);
      delete job;
      occupied = false;
    } else {
      console_respond(job->usb_id, std::move(job->result));
      delete job;
      occupied = false;
    }
  }
}
} // namespace
void start_auth() {
  auth = std::make_unique<Auth>(read_nvs("auth").text("token"),
                                read_nvs("ui_auth"), random_secret, derive,
                                persist_password, now_ms);
  jobs = xQueueCreate(1, sizeof(Job *));
  if (!jobs)
    abort();
  // Share idle priority so a long KDF leaves idle/watchdog and all instrument
  // tasks runnable, on both classic ESP32 and S3. Never hold the node mutex.
  if (xTaskCreate(worker, "password", 8192, nullptr, tskIDLE_PRIORITY,
                  nullptr) != pdPASS)
    abort();
}
Auth &authentication() { return *auth; }
Response local_auth(const std::string &method, const std::string &path,
                    const Json &body, const Headers &headers) {
  AuthContext local{true, false, ""};
  return auth->request(method, path, body, headers, local);
}
void usb_auth(const std::string &method, const std::string &path,
              const Json &body, const Headers &headers, const std::string &id) {
  auto job = std::make_unique<Job>();
  job->method = method;
  job->path = path;
  job->body = body;
  job->headers = headers;
  job->context = {true, false, ""};
  job->usb_id = id;
  if (occupied.exchange(true)) {
    console_respond(id, busy());
    return;
  }
  auto pointer = job.get();
  if (xQueueSend(jobs, &pointer, 0) != pdTRUE) {
    occupied = false;
    console_respond(id, problem(503, "Password worker unavailable"));
  } else
    job.release();
}
esp_err_t http_auth(httpd_req_t *r, const std::string &method,
                    const std::string &path, const Json &body,
                    const Headers &headers, const AuthContext &context) {
  if (method != "POST" && method != "PUT")
    return http_respond(r, auth->request(method, path, body, headers, context));
  auto job = std::make_unique<Job>();
  job->method = method;
  job->path = path;
  job->body = body;
  job->headers = headers;
  job->context = context;
  if (occupied.exchange(true))
    return http_respond(r, busy());
  if (httpd_req_async_handler_begin(r, &job->http) != ESP_OK) {
    occupied = false;
    return http_respond(r, problem(503, "Password worker unavailable"));
  }
  auto pointer = job.get();
  if (xQueueSend(jobs, &pointer, 0) != pdTRUE) {
    http_respond(job->http, problem(503, "Password worker unavailable"));
    httpd_req_async_handler_complete(job->http);
    occupied = false;
  } else
    job.release();
  return ESP_OK;
}
} // namespace labbridge::esp
