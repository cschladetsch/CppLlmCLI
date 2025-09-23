#include "http/http_client.hpp"

#include <chrono>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>

namespace llm {

HttpClient::HttpClient(const std::string &base_url, size_t timeout_sec)
    : base_url_(base_url), timeout_sec_(timeout_sec) {
  client_ = std::make_unique<httplib::Client>(base_url);
  client_->set_connection_timeout(timeout_sec);
  client_->set_read_timeout(timeout_sec);
  client_->set_write_timeout(timeout_sec);

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
  client_->enable_server_certificate_verification(true);
#endif
}

HttpClient::~HttpClient() = default;

HttpClient::Headers
HttpClient::prepare_headers(const Headers &custom_headers) const {
  Headers headers = custom_headers;
  headers["Content-Type"] = "application/json";
  headers["Accept"] = "application/json";

  if (bearer_token_) {
    headers["Authorization"] = "Bearer " + *bearer_token_;
  }

  return headers;
}

HttpClient::Response HttpClient::post(const std::string &endpoint,
                                      const nlohmann::json &data,
                                      const Headers &headers) {
  auto request_fn = [this, &endpoint, &data, &headers]() -> Response {
    auto prepared_headers = prepare_headers(headers);
    httplib::Headers httplib_headers;
    for (const auto &[key, value] : prepared_headers) {
      httplib_headers.emplace(key, value);
    }

    auto result = client_->Post(endpoint, httplib_headers, data.dump(),
                                "application/json");

    if (!result) {
      auto err = httplib::to_string(result.error());
      std::cout << "[DEBUG] HTTP Post failed: " << err << "\n";
      std::cout << "[DEBUG] URL: " << base_url_ << endpoint << "\n";
      return {0, "", {}, false, "Connection failed: " + err};
    }

    Response response;
    response.status_code = result->status;
    response.body = result->body;
    response.success = (result->status >= 200 && result->status < 300);

    for (const auto &[key, value] : result->headers) {
      response.headers[key] = value;
    }

    if (!response.success) {
      response.error =
          "HTTP " + std::to_string(result->status) + ": " + result->body;
    }

    return response;
  };

  return make_request_with_retry(request_fn);
}

HttpClient::Response HttpClient::get(const std::string &endpoint,
                                     const Headers &headers) {
  auto request_fn = [this, &endpoint, &headers]() -> Response {
    auto prepared_headers = prepare_headers(headers);
    httplib::Headers httplib_headers;
    for (const auto &[key, value] : prepared_headers) {
      httplib_headers.emplace(key, value);
    }

    auto result = client_->Get(endpoint, httplib_headers);

    if (!result) {
      auto err = httplib::to_string(result.error());
      std::cout << "[DEBUG] HTTP Post failed: " << err << "\n";
      std::cout << "[DEBUG] URL: " << base_url_ << endpoint << "\n";
      return {0, "", {}, false, "Connection failed: " + err};
    }

    Response response;
    response.status_code = result->status;
    response.body = result->body;
    response.success = (result->status >= 200 && result->status < 300);

    for (const auto &[key, value] : result->headers) {
      response.headers[key] = value;
    }

    if (!response.success) {
      response.error =
          "HTTP " + std::to_string(result->status) + ": " + result->body;
    }

    return response;
  };

  return make_request_with_retry(request_fn);
}

std::future<HttpClient::Response>
HttpClient::post_async(const std::string &endpoint, const nlohmann::json &data,
                       const Headers &headers) {
  return std::async(std::launch::async, [this, endpoint, data, headers]() {
    return post(endpoint, data, headers);
  });
}

void HttpClient::post_stream(const std::string &endpoint,
                             const nlohmann::json &data,
                             StreamCallback callback, const Headers &headers) {
  auto prepared_headers = prepare_headers(headers);
  prepared_headers["Accept"] = "text/event-stream";

  httplib::Headers httplib_headers;
  for (const auto &[key, value] : prepared_headers) {
    httplib_headers.emplace(key, value);
  }

  // Use the simple POST method without content receiver for now
  auto result =
      client_->Post(endpoint, httplib_headers, data.dump(), "application/json");
  if (result) {
    callback(result->body, true);
  }
}

void HttpClient::parse_sse_stream(const std::string &data,
                                  StreamCallback callback) {
  std::istringstream stream(data);
  std::string line;
  std::string event_data;

  while (std::getline(stream, line)) {
    if (line.starts_with("data: ")) {
      event_data = line.substr(6);

      if (event_data == "[DONE]") {
        callback("", true);
        return;
      }

      try {
        auto json_data = nlohmann::json::parse(event_data);

        if (json_data.contains("choices") &&
            json_data["choices"][0].contains("delta") &&
            json_data["choices"][0]["delta"].contains("content")) {
          std::string content = json_data["choices"][0]["delta"]["content"];
          callback(content, false);
        }
      } catch (const nlohmann::json::exception &e) {
        std::cout << "[DEBUG] Failed to parse SSE JSON: " << e.what() << "\n";
      }
    }
  }
}

HttpClient::Response
HttpClient::make_request_with_retry(std::function<Response()> request_fn) {
  for (size_t attempt = 0; attempt < retry_count_; ++attempt) {
    auto response = request_fn();

    if (response.success ||
        (response.status_code >= 400 && response.status_code < 500 &&
         response.status_code != 429)) {
      return response;
    }

    if (attempt < retry_count_ - 1) {
      size_t delay = retry_delay_ms_ * (1 << attempt);
      std::cout << "[DEBUG] Request failed, retrying in " << delay
                << " ms...\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
  }

  return request_fn();
}

void HttpClient::set_bearer_token(const std::string &token) {
  bearer_token_ = token;
}

void HttpClient::set_timeout(size_t seconds) {
  timeout_sec_ = seconds;
  client_->set_connection_timeout(seconds);
  client_->set_read_timeout(seconds);
  client_->set_write_timeout(seconds);
}

} // namespace llm