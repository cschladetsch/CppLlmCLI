#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "http/http_client.hpp"
#include "utils/test_helpers.hpp"
#include <thread>
#include <chrono>

// HTTP client tests require httplib server which is not available on Windows
#ifndef _WIN32
#include <httplib.h>

using namespace llm;
using namespace llm::test;
using namespace testing;

class HttpClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start a mock HTTP server for testing
        server_thread_ = std::thread([this]() {
            httplib::Server server;

            server.Post("/test/success", [](const httplib::Request&, httplib::Response& res) {
                res.set_content(R"({"message": "success"})", "application/json");
            });

            server.Post("/test/error", [](const httplib::Request&, httplib::Response& res) {
                res.status = 400;
                res.set_content(R"({"error": "bad request"})", "application/json");
            });

            server.Get("/test/get", [](const httplib::Request&, httplib::Response& res) {
                res.set_content(R"({"data": "test"})", "application/json");
            });

            server.Post("/test/stream", [](const httplib::Request&, httplib::Response& res) {
                res.set_header("Content-Type", "text/event-stream");
                res.set_chunked_content_provider(
                    [](size_t offset, httplib::DataSink& sink) {
                        if (offset == 0) {
                            sink.write("data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n", 51);
                            return true;
                        } else if (offset == 51) {
                            sink.write("data: {\"choices\":[{\"delta\":{\"content\":\" World\"}}]}\n\n", 53);
                            return true;
                        } else if (offset == 104) {
                            sink.write("data: [DONE]\n\n", 14);
                            return false;
                        }
                        return false;
                    }
                );
            });

            server.listen("localhost", 18080);
        });

        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        client_ = std::make_unique<HttpClient>("http://localhost:18080", 5);
    }

    void TearDown() override {
        client_.reset();
        if (server_thread_.joinable()) {
            server_thread_.detach(); // In real tests, we'd properly shut down the server
        }
    }

    std::unique_ptr<HttpClient> client_;
    std::thread server_thread_;
};

TEST_F(HttpClientTest, PostSuccessfulRequest) {
    nlohmann::json request_data = {{"test", "data"}};

    auto response = client_->post("/test/success", request_data);

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.status_code, 200);
    EXPECT_FALSE(response.body.empty());

    auto json_response = nlohmann::json::parse(response.body);
    EXPECT_EQ(json_response["message"], "success");
}

TEST_F(HttpClientTest, PostErrorRequest) {
    nlohmann::json request_data = {{"test", "data"}};

    auto response = client_->post("/test/error", request_data);

    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.status_code, 400);
    EXPECT_FALSE(response.error.empty());
}

TEST_F(HttpClientTest, GetRequest) {
    auto response = client_->get("/test/get");

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.status_code, 200);

    auto json_response = nlohmann::json::parse(response.body);
    EXPECT_EQ(json_response["data"], "test");
}

TEST_F(HttpClientTest, PostAsyncRequest) {
    nlohmann::json request_data = {{"test", "data"}};

    auto future_response = client_->post_async("/test/success", request_data);
    auto response = future_response.get();

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.status_code, 200);
}

TEST_F(HttpClientTest, StreamingRequest) {
    nlohmann::json request_data = {{"stream", true}};
    std::vector<std::string> received_chunks;

    client_->post_stream("/test/stream", request_data,
        [&received_chunks](const std::string& chunk, bool is_done) {
            received_chunks.push_back(chunk);
            if (is_done) {
                // Stream is complete
            }
        });

    EXPECT_FALSE(received_chunks.empty());

    // Should receive chunks with content
    std::string combined_content;
    for (const auto& chunk : received_chunks) {
        combined_content += chunk;
    }
    EXPECT_THAT(combined_content, HasSubstr("Hello"));
    EXPECT_THAT(combined_content, HasSubstr("World"));
}

TEST_F(HttpClientTest, SetBearerToken) {
    std::string test_token = "test-bearer-token";

    EXPECT_NO_THROW(client_->set_bearer_token(test_token));

    // Test that subsequent requests include the token
    nlohmann::json request_data = {{"test", "data"}};
    auto response = client_->post("/test/success", request_data);

    // We can't easily verify the header was sent in this test setup,
    // but we can verify the call doesn't fail
    EXPECT_TRUE(response.success || response.status_code > 0);
}

TEST_F(HttpClientTest, SetTimeout) {
    EXPECT_NO_THROW(client_->set_timeout(10));

    // Test that requests still work with new timeout
    auto response = client_->get("/test/get");
    EXPECT_TRUE(response.success || response.status_code > 0);
}

TEST_F(HttpClientTest, RetryConfiguration) {
    EXPECT_NO_THROW(client_->set_retry_count(5));
    EXPECT_NO_THROW(client_->set_retry_delay(100));

    // Test that configuration doesn't break basic functionality
    auto response = client_->get("/test/get");
    EXPECT_TRUE(response.success || response.status_code > 0);
}

TEST_F(HttpClientTest, CustomHeaders) {
    HttpClient::Headers custom_headers = {
        {"X-Custom-Header", "test-value"},
        {"X-Another-Header", "another-value"}
    };

    nlohmann::json request_data = {{"test", "data"}};
    auto response = client_->post("/test/success", request_data, custom_headers);

    EXPECT_TRUE(response.success);
}

TEST_F(HttpClientTest, InvalidUrl) {
    HttpClient invalid_client("http://invalid-host:99999", 1);
    invalid_client.set_retry_count(1); // Reduce retries for faster test

    nlohmann::json request_data = {{"test", "data"}};
    auto response = invalid_client.post("/test", request_data);

    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.status_code, 0);
    EXPECT_FALSE(response.error.empty());
}

// Test JSON request/response handling
class HttpClientJsonTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<HttpClient>("http://httpbin.org", 10);
    }

    std::unique_ptr<HttpClient> client_;
};

TEST_F(HttpClientJsonTest, JsonRequestSerialization) {
    nlohmann::json complex_data = {
        {"string_field", "test"},
        {"number_field", 42},
        {"bool_field", true},
        {"array_field", nlohmann::json::array({1, 2, 3})},
        {"object_field", {
            {"nested", "value"}
        }}
    };

    // This test will make a real HTTP request to httpbin.org
    // Skip if not connected to internet
    try {
        auto response = client_->post("/post", complex_data);
        if (response.success) {
            auto response_json = nlohmann::json::parse(response.body);
            EXPECT_TRUE(response_json.contains("json"));
            EXPECT_EQ(response_json["json"]["string_field"], "test");
            EXPECT_EQ(response_json["json"]["number_field"], 42);
        }
    } catch (const std::exception&) {
        GTEST_SKIP() << "Skipping test requiring internet connection";
    }
}

#else // _WIN32

// Placeholder test for Windows where httplib server is not available
TEST(HttpClientTestWindows, Placeholder) {
    GTEST_SKIP() << "HTTP client tests are not available on Windows (no httplib server support)";
}

#endif // _WIN32