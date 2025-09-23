#ifdef _WIN32

#include "utils/logger.hpp"
#include "http/http_client.hpp"
#include <windows.h>
#include <winhttp.h>
#include <iostream>
#include <sstream>
#include <vector>

#pragma comment(lib, "winhttp.lib")

namespace llm {

class WinHttpClient {
private:
    HINTERNET hSession = nullptr;
    HINTERNET hConnect = nullptr;
    std::string host_;
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

    HttpClient::Response post(const std::string& path, const std::string& data) {
        HttpClient::Response response;

        // Convert path to wide string
        int path_size = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
        std::vector<wchar_t> wpath(path_size);
        MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), path_size);

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

// Wrapper implementation for HttpClient using WinHTTP
HttpClient::HttpClient(const std::string& base_url, size_t timeout_sec)
    : base_url_(base_url), timeout_sec_(timeout_sec) {
    retry_count_ = 3;
    retry_delay_ms_ = 1000;
}

HttpClient::~HttpClient() = default;

void HttpClient::set_bearer_token(const std::string& token) {
    bearer_token_ = token;
}

HttpClient::Response HttpClient::post(const std::string& endpoint,
                                      const nlohmann::json& data,
                                      const Headers& headers) {
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
}

HttpClient::Response HttpClient::get(const std::string& endpoint,
                                     const Headers& headers) {
    // Not implemented for now - we only need POST for the API
    return {0, "", {}, false, "GET not implemented"};
}

std::future<HttpClient::Response> HttpClient::post_async(const std::string& endpoint,
                                                         const nlohmann::json& data,
                                                         const Headers& headers) {
    return std::async(std::launch::async, [this, endpoint, data, headers]() {
        return post(endpoint, data, headers);
    });
}

void HttpClient::post_stream(const std::string& endpoint,
                            const nlohmann::json& data,
                            StreamCallback callback,
                            const Headers& headers) {
    // Streaming not implemented for WinHTTP version
    auto response = post(endpoint, data, headers);
    if (response.success) {
        callback(response.body, true);
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
            LOG_WARN("Request failed, retrying in {} ms...", delay);
            Sleep(delay);
        }
    }

    return request_fn();
}

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

void HttpClient::set_timeout(size_t seconds) {
    timeout_sec_ = seconds;
}

void HttpClient::set_retry_count(size_t count) {
    retry_count_ = count;
}

void HttpClient::set_retry_delay(size_t ms) {
    retry_delay_ms_ = ms;
}

} // namespace llm

#endif // _WIN32