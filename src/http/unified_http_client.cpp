#include "http/http_client.hpp"
#include "utils/logger.hpp"

#include <chrono>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#include <vector>
#pragma comment(lib, "winhttp.lib")
#else
#include <httplib.h>
#endif

namespace llm {

#ifdef _WIN32
// Windows implementation using WinHTTP

class WinHttpClient {
private:
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    std::string host_;
    std::string base_path_;
    int port_;
    bool is_https_;
    std::string bearer_token_;

public:
    WinHttpClient(const std::string& base_url) {
        // Parse URL
        std::string url = base_url;
        is_https_ = (url.find("https://") == 0);
        if (is_https_) {
            url = url.substr(8);
            port_ = INTERNET_DEFAULT_HTTPS_PORT;
        } else if (url.find("http://") == 0) {
            url = url.substr(7);
            port_ = INTERNET_DEFAULT_HTTP_PORT;
        }

        // Extract host and port
        size_t port_pos = url.find(':');
        size_t path_pos = url.find('/');

        if (port_pos != std::string::npos && (path_pos == std::string::npos || port_pos < path_pos)) {
            host_ = url.substr(0, port_pos);
            std::string port_str = url.substr(port_pos + 1,
                (path_pos != std::string::npos) ? path_pos - port_pos - 1 : std::string::npos);
            port_ = std::stoi(port_str);
        } else if (path_pos != std::string::npos) {
            host_ = url.substr(0, path_pos);
        } else {
            host_ = url;
        }

        // Extract base path
        if (path_pos != std::string::npos) {
            if (port_pos != std::string::npos && port_pos > path_pos) {
                // Port comes after path (shouldn't happen in URLs but handle it)
                base_path_ = url.substr(path_pos);
            } else {
                base_path_ = url.substr(path_pos);
            }
        } else {
            base_path_ = "";
        }

        // Initialize WinHTTP
        hSession = WinHttpOpen(L"LLM-REPL/1.0",
                              WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);

        if (!hSession) {
            throw std::runtime_error("Failed to initialize WinHTTP");
        }

        // Convert host to wide string
        int size = MultiByteToWideChar(CP_UTF8, 0, host_.c_str(), -1, nullptr, 0);
        std::vector<wchar_t> whost(size);
        MultiByteToWideChar(CP_UTF8, 0, host_.c_str(), -1, whost.data(), size);

        // Connect to server
        hConnect = WinHttpConnect(hSession, whost.data(), port_, 0);
        if (!hConnect) {
            WinHttpCloseHandle(hSession);
            throw std::runtime_error("Failed to connect to " + host_);
        }
    }

    ~WinHttpClient() {
        if (hConnect) WinHttpCloseHandle(hConnect);
        if (hSession) WinHttpCloseHandle(hSession);
    }

    void set_bearer_token(const std::string& token) {
        bearer_token_ = token;
    }

    HttpClient::Response get(const std::string& path) {
        HttpClient::Response response;

        // Combine base path with requested path
        std::string full_path = base_path_ + path;

        // Convert path to wide string
        int path_size = MultiByteToWideChar(CP_UTF8, 0, full_path.c_str(), -1, nullptr, 0);
        std::vector<wchar_t> wpath(path_size);
        MultiByteToWideChar(CP_UTF8, 0, full_path.c_str(), -1, wpath.data(), path_size);

        // Create request
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wpath.data(),
                                               nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               is_https_ ? WINHTTP_FLAG_SECURE : 0);

        if (!hRequest) {
            response.success = false;
            response.error = "Failed to create request";
            return response;
        }

        // Add headers
        std::wstring headers = L"Accept: application/json\r\n";
        if (!bearer_token_.empty()) {
            headers += L"Authorization: Bearer ";
            int token_size = MultiByteToWideChar(CP_UTF8, 0, bearer_token_.c_str(), -1, nullptr, 0);
            std::vector<wchar_t> wtoken(token_size);
            MultiByteToWideChar(CP_UTF8, 0, bearer_token_.c_str(), -1, wtoken.data(), token_size);
            headers += wtoken.data();
            headers += L"\r\n";
        }

        WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

        // Send request
        BOOL result = WinHttpSendRequest(hRequest,
                                        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                        nullptr, 0, 0, 0);

        if (result) {
            result = WinHttpReceiveResponse(hRequest, nullptr);
        }

        if (!result) {
            response.success = false;
            response.error = "Failed to send request";
            WinHttpCloseHandle(hRequest);
            return response;
        }

        // Get status code
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
                           WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX,
                           &statusCode, &statusCodeSize,
                           WINHTTP_NO_HEADER_INDEX);

        response.status_code = statusCode;

        // Read response
        std::string responseData;
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;

        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                break;
            }

            if (dwSize == 0) {
                break;
            }

            std::vector<char> buffer(dwSize + 1);
            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                break;
            }

            buffer[dwDownloaded] = '\0';
            responseData.append(buffer.data(), dwDownloaded);
        } while (dwSize > 0);

        response.body = responseData;
        response.success = (statusCode >= 200 && statusCode < 300);

        if (!response.success && response.body.empty()) {
            response.error = "HTTP " + std::to_string(statusCode);
        }

        WinHttpCloseHandle(hRequest);
        return response;
    }

    HttpClient::Response post(const std::string& path, const std::string& data) {
        HttpClient::Response response;

        // Combine base path with requested path
        std::string full_path = base_path_ + path;

        // Convert path to wide string
        int path_size = MultiByteToWideChar(CP_UTF8, 0, full_path.c_str(), -1, nullptr, 0);
        std::vector<wchar_t> wpath(path_size);
        MultiByteToWideChar(CP_UTF8, 0, full_path.c_str(), -1, wpath.data(), path_size);

        // Create request
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wpath.data(),
                                               nullptr, WINHTTP_NO_REFERER,
                                               WINHTTP_DEFAULT_ACCEPT_TYPES,
                                               is_https_ ? WINHTTP_FLAG_SECURE : 0);

        if (!hRequest) {
            response.success = false;
            response.error = "Failed to create request";
            return response;
        }

        // Add headers
        std::wstring headers = L"Content-Type: application/json\r\n";
        if (!bearer_token_.empty()) {
            headers += L"Authorization: Bearer ";
            int token_size = MultiByteToWideChar(CP_UTF8, 0, bearer_token_.c_str(), -1, nullptr, 0);
            std::vector<wchar_t> wtoken(token_size);
            MultiByteToWideChar(CP_UTF8, 0, bearer_token_.c_str(), -1, wtoken.data(), token_size);
            headers += wtoken.data();
            headers += L"\r\n";
        }

        WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1, WINHTTP_ADDREQ_FLAG_ADD);

        // Send request
        BOOL result = WinHttpSendRequest(hRequest,
                                        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                        (LPVOID)data.c_str(), data.length(),
                                        data.length(), 0);

        if (result) {
            result = WinHttpReceiveResponse(hRequest, nullptr);
        }

        if (!result) {
            response.success = false;
            response.error = "Failed to send request";
            WinHttpCloseHandle(hRequest);
            return response;
        }

        // Get status code
        DWORD statusCode = 0;
        DWORD statusCodeSize = sizeof(statusCode);
        WinHttpQueryHeaders(hRequest,
                           WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX,
                           &statusCode, &statusCodeSize,
                           WINHTTP_NO_HEADER_INDEX);

        response.status_code = statusCode;

        // Read response
        std::string responseData;
        DWORD dwSize = 0;
        DWORD dwDownloaded = 0;

        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) {
                break;
            }

            if (dwSize == 0) {
                break;
            }

            std::vector<char> buffer(dwSize + 1);
            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded)) {
                break;
            }

            buffer[dwDownloaded] = '\0';
            responseData.append(buffer.data(), dwDownloaded);
        } while (dwSize > 0);

        response.body = responseData;
        response.success = (statusCode >= 200 && statusCode < 300);

        if (!response.success && response.body.empty()) {
            response.error = "HTTP " + std::to_string(statusCode);
        }

        WinHttpCloseHandle(hRequest);
        return response;
    }
};

#endif // _WIN32

// Common HttpClient implementation

HttpClient::HttpClient(const std::string& base_url, size_t timeout_sec)
    : base_url_(base_url), timeout_sec_(timeout_sec) {

#ifdef _WIN32
    retry_count_ = 3;
    retry_delay_ms_ = 1000;
#else
    spdlog::debug("HttpClient initializing with URL: {}", base_url);
    spdlog::debug("Timeout set to: {} seconds", timeout_sec);

    client_ = std::make_unique<httplib::Client>(base_url);
    client_->set_connection_timeout(timeout_sec);
    client_->set_read_timeout(timeout_sec);
    client_->set_write_timeout(timeout_sec);

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    spdlog::debug("SSL support enabled");
    client_->enable_server_certificate_verification(true);
#else
    spdlog::warn("SSL support NOT enabled");
#endif
#endif
}

HttpClient::~HttpClient() = default;

HttpClient::Headers
HttpClient::prepare_headers(const Headers& custom_headers) const {
    Headers headers = custom_headers;
    headers["Content-Type"] = "application/json";
    headers["Accept"] = "application/json";

    if (bearer_token_) {
        headers["Authorization"] = "Bearer " + *bearer_token_;
    }

    return headers;
}

HttpClient::Response HttpClient::post(const std::string& endpoint,
                                      const nlohmann::json& data,
                                      const Headers& headers) {
#ifdef _WIN32
    auto request_fn = [this, &endpoint, &data]() -> Response {
        try {
            WinHttpClient client(base_url_);
            if (bearer_token_) {
                client.set_bearer_token(*bearer_token_);
            }

            auto response = client.post(endpoint, data.dump());

            if (!response.success) {
                LOG_ERROR("HTTP Post failed with status: {}", response.status_code);
                LOG_ERROR("URL: {}{}", base_url_, endpoint);
                if (!response.body.empty()) {
                    LOG_ERROR("Response: {}", response.body.substr(0, 200));
                }
            }

            return response;
        } catch (const std::exception& e) {
            LOG_ERROR("HTTP Post exception: {}", e.what());
            LOG_ERROR("URL: {}{}", base_url_, endpoint);
            return {0, "", {}, false, std::string("Connection failed: ") + e.what()};
        }
    };

    return make_request_with_retry(request_fn);
#else
    auto request_fn = [this, &endpoint, &data, &headers]() -> Response {
        auto prepared_headers = prepare_headers(headers);
        httplib::Headers httplib_headers;
        for (const auto& [key, value] : prepared_headers) {
            httplib_headers.emplace(key, value);
        }

        spdlog::debug("POST request to: {}{}", base_url_, endpoint);
        spdlog::debug("Request body size: {} bytes", data.dump().length());

        auto result = client_->Post(endpoint, httplib_headers, data.dump(),
                                    "application/json");

        if (!result) {
            std::string error_msg = "Connection failed to " + base_url_ + endpoint;
            spdlog::error("{}", error_msg);
            spdlog::debug("Failed to connect to: {}{}", base_url_, endpoint);
            return {0, "", {}, false, error_msg};
        }

        Response response;
        response.status_code = result->status;
        response.body = result->body;
        response.success = (result->status >= 200 && result->status < 300);

        for (const auto& [key, value] : result->headers) {
            response.headers[key] = value;
        }

        if (!response.success) {
            response.error =
                "HTTP " + std::to_string(result->status) + ": " + result->body;
        }

        return response;
    };

    return make_request_with_retry(request_fn);
#endif
}

HttpClient::Response HttpClient::get(const std::string& endpoint,
                                     const Headers& headers) {
#ifdef _WIN32
    auto request_fn = [this, &endpoint]() -> Response {
        try {
            WinHttpClient client(base_url_);
            if (bearer_token_) {
                client.set_bearer_token(*bearer_token_);
            }

            auto response = client.get(endpoint);

            if (!response.success) {
                LOG_ERROR("HTTP Get failed with status: {}", response.status_code);
                LOG_ERROR("URL: {}{}", base_url_, endpoint);
                if (!response.body.empty()) {
                    LOG_ERROR("Response: {}", response.body.substr(0, 200));
                }
            }

            return response;
        } catch (const std::exception& e) {
            LOG_ERROR("HTTP Get exception: {}", e.what());
            LOG_ERROR("URL: {}{}", base_url_, endpoint);
            return {0, "", {}, false, std::string("Connection failed: ") + e.what()};
        }
    };

    return make_request_with_retry(request_fn);
#else
    auto request_fn = [this, &endpoint, &headers]() -> Response {
        auto prepared_headers = prepare_headers(headers);
        httplib::Headers httplib_headers;
        for (const auto& [key, value] : prepared_headers) {
            httplib_headers.emplace(key, value);
        }

        spdlog::debug("GET request to: {}{}", base_url_, endpoint);

        auto result = client_->Get(endpoint, httplib_headers);

        if (!result) {
            std::string error_msg = "Connection failed to " + base_url_ + endpoint;
            spdlog::error("{}", error_msg);
            spdlog::debug("Failed to connect to: {}{}", base_url_, endpoint);
            return {0, "", {}, false, error_msg};
        }

        Response response;
        response.status_code = result->status;
        response.body = result->body;
        response.success = (result->status >= 200 && result->status < 300);

        for (const auto& [key, value] : result->headers) {
            response.headers[key] = value;
        }

        if (!response.success) {
            response.error =
                "HTTP " + std::to_string(result->status) + ": " + result->body;
        }

        return response;
    };

    return make_request_with_retry(request_fn);
#endif
}

std::future<HttpClient::Response>
HttpClient::post_async(const std::string& endpoint, const nlohmann::json& data,
                       const Headers& headers) {
    return std::async(std::launch::async, [this, endpoint, data, headers]() {
        return post(endpoint, data, headers);
    });
}

void HttpClient::post_stream(const std::string& endpoint,
                             const nlohmann::json& data,
                             StreamCallback callback, const Headers& headers) {
#ifdef _WIN32
    // Streaming not implemented for WinHTTP version
    auto response = post(endpoint, data, headers);
    if (response.success) {
        callback(response.body, true);
    }
#else
    auto prepared_headers = prepare_headers(headers);
    prepared_headers["Accept"] = "text/event-stream";

    httplib::Headers httplib_headers;
    for (const auto& [key, value] : prepared_headers) {
        httplib_headers.emplace(key, value);
    }

    // Use the simple POST method without content receiver for now
    auto result =
        client_->Post(endpoint, httplib_headers, data.dump(), "application/json");
    if (result) {
        callback(result->body, true);
    }
#endif
}

#ifndef _WIN32
void HttpClient::parse_sse_stream(const std::string& data,
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
            } catch (const nlohmann::json::exception& e) {
                spdlog::debug("Failed to parse SSE JSON: {}", e.what());
            }
        }
    }
}
#endif

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
#ifdef _WIN32
            LOG_WARN("Request failed, retrying in {} ms...", delay);
            Sleep(delay);
#else
            spdlog::debug("Request failed, retrying in {} ms...", delay);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
#endif
        }
    }

    return request_fn();
}

void HttpClient::set_bearer_token(const std::string& token) {
    bearer_token_ = token;
#ifndef _WIN32
    if (!token.empty()) {
        spdlog::debug("Bearer token set (length: {})", token.length());
    } else {
        spdlog::warn("Bearer token is empty!");
    }
#endif
}

void HttpClient::set_timeout(size_t seconds) {
    timeout_sec_ = seconds;
#ifndef _WIN32
    client_->set_connection_timeout(seconds);
    client_->set_read_timeout(seconds);
    client_->set_write_timeout(seconds);
#endif
}

void HttpClient::set_retry_count(size_t count) {
    retry_count_ = count;
}

void HttpClient::set_retry_delay(size_t ms) {
    retry_delay_ms_ = ms;
}

} // namespace llm