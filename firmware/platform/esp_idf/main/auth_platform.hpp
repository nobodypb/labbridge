#pragma once
#include "esp_http_server.h"
#include "labbridge/auth.hpp"
namespace labbridge::esp {
Headers http_headers(httpd_req_t *);
AuthContext http_auth_context(httpd_req_t *, const Headers &);
void start_auth();
Auth &authentication();
Response local_auth(const std::string &, const std::string &, const Json &,
                    const Headers &);
void usb_auth(const std::string &, const std::string &, const Json &,
              const Headers &, const std::string &id);
esp_err_t http_auth(httpd_req_t *, const std::string &, const std::string &,
                    const Json &, const Headers &, const AuthContext &);
esp_err_t http_respond(httpd_req_t *, Response);
} // namespace labbridge::esp
