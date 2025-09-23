#pragma once

// httplib not needed for Windows build

#include <functional>
#include <future>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace llm {

class HttpClient {
public:
  struct Response {
    int status_code;
    std::string body;
    std::map<std::string, std::string> headers;
    bool success;
    std::string error;
  };

  using StreamCallback =
      std::function<void(const std::string &chunk, bool is_done)>;
  using Headers = std::map<std::string, std::string>;

  HttpClient(const std::string &base_url, size_t timeout_sec = 30);
  ~HttpClient();

  Response post(const std::string &endpoint, const nlohmann::json &data,
                const Headers &headers = {});

  Response get(const std::string &endpoint, const Headers &headers = {});

  std::future<Response> post_async(const std::string &endpoint,
                                   const nlohmann::json &data,
                                   const Headers &headers = {});

  void post_stream(const std::string &endpoint, const nlohmann::json &data,
                   StreamCallback callback, const Headers &headers = {});

  void set_bearer_token(const std::string &token);
  void set_timeout(size_t seconds);
  void set_retry_count(size_t count);
  void set_retry_delay(size_t milliseconds);

private:
  // httplib client not needed for Windows
  std::string base_url_;
  std::optional<std::string> bearer_token_;
  size_t timeout_sec_;
  size_t retry_count_ = 3;
  size_t retry_delay_ms_ = 1000;

  Headers prepare_headers(const Headers &custom_headers) const;
  Response make_request_with_retry(std::function<Response()> request_fn);
  void parse_sse_stream(const std::string &data, StreamCallback callback);
};

} // namespace llm