#pragma once

#include "http/http_client.hpp"
#include <gmock/gmock.h>

namespace llm {

class MockHttpClient : public HttpClient {
public:
    MockHttpClient() : HttpClient("http://localhost:8080") {}

    MOCK_METHOD(Response, post, (const std::string& endpoint,
                                const nlohmann::json& data,
                                const Headers& headers));

    MOCK_METHOD(Response, get, (const std::string& endpoint,
                               const Headers& headers));

    MOCK_METHOD(std::future<Response>, post_async, (const std::string& endpoint,
                                                   const nlohmann::json& data,
                                                   const Headers& headers));

    MOCK_METHOD(void, post_stream, (const std::string& endpoint,
                                   const nlohmann::json& data,
                                   StreamCallback callback,
                                   const Headers& headers));

    MOCK_METHOD(void, set_bearer_token, (const std::string& token));
    MOCK_METHOD(void, set_timeout, (size_t seconds));
    MOCK_METHOD(void, set_retry_count, (size_t count));
    MOCK_METHOD(void, set_retry_delay, (size_t milliseconds));

    // Helper methods for setting up responses
    void SetupSuccessfulPostResponse(const std::string& content) {
        Response response;
        response.status_code = 200;
        response.success = true;
        response.body = R"({"choices":[{"message":{"content":")" + content + R"("}}]})";

        EXPECT_CALL(*this, post(testing::_, testing::_, testing::_))
            .WillRepeatedly(testing::Return(response));
    }

    void SetupErrorResponse(int status_code, const std::string& error) {
        Response response;
        response.status_code = status_code;
        response.success = false;
        response.error = error;
        response.body = R"({"error":{"message":")" + error + R"("}})";

        EXPECT_CALL(*this, post(testing::_, testing::_, testing::_))
            .WillRepeatedly(testing::Return(response));
    }

    void SetupStreamingResponse(const std::vector<std::string>& chunks) {
        EXPECT_CALL(*this, post_stream(testing::_, testing::_, testing::_, testing::_))
            .WillOnce(testing::Invoke([chunks](const std::string&, const nlohmann::json&,
                                             StreamCallback callback, const Headers&) {
                for (const auto& chunk : chunks) {
                    callback(chunk, false);
                }
                callback("", true); // Signal end of stream
            }));
    }
};

} // namespace llm