#include <gtest/gtest.h>
#include "llm/groq_service.hpp"
#include "http/http_client.hpp"
#include "utils/test_helpers.hpp"
#include <httplib.h>
#include <thread>
#include <chrono>

using namespace llm;
using namespace llm::test;

class LLMHttpIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Start a mock API server that mimics LLM provider responses
        StartMockServer();

        // Give server time to start
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        service_ = std::make_unique<GroqService>("test-api-key", "http://localhost:18081");
    }

    void TearDown() override {
        service_.reset();
        StopMockServer();
    }

private:
    void StartMockServer() {
        server_thread_ = std::thread([this]() {
            httplib::Server server;

            // Mock chat completions endpoint
            server.Post("/openai/v1/chat/completions", [](const httplib::Request& req, httplib::Response& res) {
                auto json_req = nlohmann::json::parse(req.body);

                // Check if it's a streaming request
                if (json_req.contains("stream") && json_req["stream"] == true) {
                    res.set_header("Content-Type", "text/event-stream");
                    res.set_content(
                        "data: {\"choices\":[{\"delta\":{\"content\":\"Hello\"}}]}\n\n"
                        "data: {\"choices\":[{\"delta\":{\"content\":\" from\"}}]}\n\n"
                        "data: {\"choices\":[{\"delta\":{\"content\":\" mock\"}}]}\n\n"
                        "data: {\"choices\":[{\"delta\":{\"content\":\" API\"}}]}\n\n"
                        "data: [DONE]\n\n",
                        "text/event-stream"
                    );
                } else {
                    // Regular completion
                    nlohmann::json response = {
                        {"choices", nlohmann::json::array({
                            {
                                {"message", {
                                    {"role", "assistant"},
                                    {"content", "Hello from mock API"}
                                }}
                            }
                        })},
                        {"usage", {
                            {"total_tokens", 50},
                            {"prompt_tokens", 20},
                            {"completion_tokens", 30}
                        }},
                        {"model", "mock-model"}
                    };

                    res.set_content(response.dump(), "application/json");
                }
            });

            // Mock models endpoint
            server.Get("/openai/v1/models", [](const httplib::Request&, httplib::Response& res) {
                nlohmann::json response = {
                    {"data", nlohmann::json::array({
                        {
                            {"id", "mock-model-1"},
                            {"object", "model"},
                            {"created", 1234567890}
                        },
                        {
                            {"id", "mock-model-2"},
                            {"object", "model"},
                            {"created", 1234567891}
                        }
                    })}
                };

                res.set_content(response.dump(), "application/json");
            });

            // Mock error endpoint
            server.Post("/openai/v1/error", [](const httplib::Request&, httplib::Response& res) {
                res.status = 400;
                nlohmann::json error_response = {
                    {"error", {
                        {"message", "Mock API error"},
                        {"type", "invalid_request_error"},
                        {"code", "invalid_api_key"}
                    }}
                };
                res.set_content(error_response.dump(), "application/json");
            });

            // Rate limit endpoint
            server.Post("/openai/v1/rate_limit", [](const httplib::Request&, httplib::Response& res) {
                res.status = 429;
                res.set_header("Retry-After", "1");
                nlohmann::json error_response = {
                    {"error", {
                        {"message", "Rate limit exceeded"},
                        {"type", "rate_limit_error"}
                    }}
                };
                res.set_content(error_response.dump(), "application/json");
            });

            server.listen("localhost", 18081);
        });
    }

    void StopMockServer() {
        if (server_thread_.joinable()) {
            server_thread_.detach();
        }
    }

    std::unique_ptr<GroqService> service_;
    std::thread server_thread_;
};

TEST_F(LLMHttpIntegrationTest, BasicCompletion) {
    Conversation conv;
    conv.add_user("Hello, world!");

    auto response = service_->complete(conv);

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.content, "Hello from mock API");
    EXPECT_EQ(response.model, "mock-model");
    EXPECT_EQ(response.tokens_used, 50);
    EXPECT_TRUE(response.error.empty());
}

TEST_F(LLMHttpIntegrationTest, StringCompletion) {
    auto response = service_->complete("Test prompt");

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.content, "Hello from mock API");
    EXPECT_FALSE(response.model.empty());
}

TEST_F(LLMHttpIntegrationTest, AsyncCompletion) {
    Conversation conv;
    conv.add_user("Async test");

    auto future_response = service_->complete_async(conv);
    auto response = future_response.get();

    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.content, "Hello from mock API");
}

TEST_F(LLMHttpIntegrationTest, StreamingCompletion) {
    std::vector<std::string> chunks;
    bool completion_received = false;

    service_->stream_complete("Stream test",
        [&chunks, &completion_received](const std::string& chunk, bool is_done) {
            if (!is_done) {
                chunks.push_back(chunk);
            } else {
                completion_received = true;
            }
        });

    EXPECT_FALSE(chunks.empty());
    EXPECT_TRUE(completion_received);

    // Verify we received the expected chunks
    std::string full_response;
    for (const auto& chunk : chunks) {
        full_response += chunk;
    }
    EXPECT_EQ(full_response, "Hello from mock API");
}

TEST_F(LLMHttpIntegrationTest, ConversationWithHistory) {
    Conversation conv;
    conv.add_system("You are a helpful assistant.");
    conv.add_user("First message");
    conv.add_assistant("First response");
    conv.add_user("Second message");

    auto response = service_->complete(conv);

    EXPECT_TRUE(response.success);
    EXPECT_FALSE(response.content.empty());
}

TEST_F(LLMHttpIntegrationTest, ModelConfiguration) {
    // Test model switching
    service_->set_model("llama-3.1-8b-instant");
    EXPECT_EQ(service_->get_current_model(), "llama-3.1-8b-instant");

    auto response = service_->complete("Test with different model");
    EXPECT_TRUE(response.success);
}

TEST_F(LLMHttpIntegrationTest, ParameterConfiguration) {
    service_->set_temperature(0.9f);
    service_->set_max_tokens(1024);

    auto response = service_->complete("Test with custom parameters");
    EXPECT_TRUE(response.success);
}

TEST_F(LLMHttpIntegrationTest, SystemPromptIntegration) {
    service_->set_system_prompt("You are a test assistant.");

    auto response = service_->complete("Hello");
    EXPECT_TRUE(response.success);
}

TEST_F(LLMHttpIntegrationTest, MultipleSequentialRequests) {
    for (int i = 0; i < 5; ++i) {
        auto response = service_->complete("Request " + std::to_string(i));
        EXPECT_TRUE(response.success);
        EXPECT_EQ(response.content, "Hello from mock API");
    }
}

TEST_F(LLMHttpIntegrationTest, ConcurrentRequests) {
    std::vector<std::future<CompletionResponse>> futures;

    // Launch multiple async requests
    for (int i = 0; i < 3; ++i) {
        futures.push_back(service_->complete_async(TestHelpers::CreateTestConversation()));
    }

    // Wait for all to complete
    for (auto& future : futures) {
        auto response = future.get();
        EXPECT_TRUE(response.success);
        EXPECT_EQ(response.content, "Hello from mock API");
    }
}

TEST_F(LLMHttpIntegrationTest, LargeConversation) {
    Conversation conv;
    conv.add_system("System prompt");

    // Add many message pairs
    for (int i = 0; i < 20; ++i) {
        conv.add_user("User message " + std::to_string(i));
        conv.add_assistant("Assistant response " + std::to_string(i));
    }

    conv.add_user("Final question");

    auto response = service_->complete(conv);
    EXPECT_TRUE(response.success);
}

TEST_F(LLMHttpIntegrationTest, EmptyAndSpecialMessages) {
    Conversation conv;
    conv.add_user(""); // Empty message
    conv.add_user("   "); // Whitespace only
    conv.add_user("Message with\nnewlines\tand\ttabs");
    conv.add_user("Unicode message: 🚀 🤖 こんにちは");

    auto response = service_->complete(conv);
    EXPECT_TRUE(response.success);
}

// Test HTTP-specific behaviors
class HttpClientIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_unique<HttpClient>("http://localhost:18081", 5);

        // Start mock server
        server_thread_ = std::thread([this]() {
            httplib::Server server;

            server.Post("/test", [](const httplib::Request& req, httplib::Response& res) {
                // Echo back request headers and body info
                nlohmann::json response = {
                    {"headers_received", req.headers.size()},
                    {"body_size", req.body.size()},
                    {"content_type", req.get_header_value("Content-Type")},
                    {"authorization", req.get_header_value("Authorization")}
                };
                res.set_content(response.dump(), "application/json");
            });

            server.listen("localhost", 18081);
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    void TearDown() override {
        client_.reset();
        if (server_thread_.joinable()) {
            server_thread_.detach();
        }
    }

    std::unique_ptr<HttpClient> client_;
    std::thread server_thread_;
};

TEST_F(HttpClientIntegrationTest, AuthenticationHeaders) {
    client_->set_bearer_token("test-token-123");

    nlohmann::json request_data = {{"test", "data"}};
    auto response = client_->post("/test", request_data);

    EXPECT_TRUE(response.success);

    auto response_json = nlohmann::json::parse(response.body);
    EXPECT_EQ(response_json["authorization"], "Bearer test-token-123");
    EXPECT_EQ(response_json["content_type"], "application/json");
}

TEST_F(HttpClientIntegrationTest, CustomHeaders) {
    HttpClient::Headers custom_headers = {
        {"X-Custom-Header", "custom-value"},
        {"X-API-Version", "v1"}
    };

    nlohmann::json request_data = {{"test", "data"}};
    auto response = client_->post("/test", request_data, custom_headers);

    EXPECT_TRUE(response.success);

    // Verify request was received with proper content type
    auto response_json = nlohmann::json::parse(response.body);
    EXPECT_EQ(response_json["content_type"], "application/json");
    EXPECT_GT(response_json["headers_received"].get<int>(), 0);
}

TEST_F(HttpClientIntegrationTest, JsonSerialization) {
    nlohmann::json complex_data = {
        {"string", "test"},
        {"number", 42},
        {"boolean", true},
        {"array", nlohmann::json::array({1, 2, 3})},
        {"object", {{"nested", "value"}}}
    };

    auto response = client_->post("/test", complex_data);

    EXPECT_TRUE(response.success);

    auto response_json = nlohmann::json::parse(response.body);
    EXPECT_GT(response_json["body_size"].get<int>(), 0);
}