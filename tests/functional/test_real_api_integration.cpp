#include <gtest/gtest.h>
#include "llm/groq_service.hpp"
#include "utils/test_helpers.hpp"
#include <cstdlib>

using namespace llm;
using namespace llm::test;

// These tests require real API access and should be run separately
// They can be skipped if API keys are not available
class RealAPIIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Check if API key is available
        const char* api_key = std::getenv("GROQ_API_KEY");
        if (!api_key || std::string(api_key).empty()) {
            GTEST_SKIP() << "Skipping real API tests - GROQ_API_KEY not set";
        }

        api_key_ = api_key;
        service_ = std::make_unique<GroqService>(api_key_);

        // Verify service is available
        if (!service_->is_available()) {
            GTEST_SKIP() << "Skipping real API tests - service not available";
        }
    }

    std::string api_key_;
    std::unique_ptr<GroqService> service_;
};

TEST_F(RealAPIIntegrationTest, BasicCompletion) {
    auto response = service_->complete("Say 'Hello, World!' exactly and nothing else.");

    EXPECT_TRUE(response.success) << "API call failed: " << response.error;
    EXPECT_FALSE(response.content.empty());
    EXPECT_THAT(response.content, HasSubstr("Hello"));
    EXPECT_GT(response.tokens_used, 0);
    EXPECT_FALSE(response.model.empty());
}

TEST_F(RealAPIIntegrationTest, ConversationCompletion) {
    Conversation conv;
    conv.add_system("You are a helpful assistant. Keep responses brief.");
    conv.add_user("What is 2+2?");

    auto response = service_->complete(conv);

    EXPECT_TRUE(response.success) << "API call failed: " << response.error;
    EXPECT_FALSE(response.content.empty());
    EXPECT_THAT(response.content, AnyOf(HasSubstr("4"), HasSubstr("four")));
}

TEST_F(RealAPIIntegrationTest, AsyncCompletion) {
    auto future_response = service_->complete_async(
        TestHelpers::CreateTestConversation()
    );

    auto response = future_response.get();

    EXPECT_TRUE(response.success) << "Async API call failed: " << response.error;
    EXPECT_FALSE(response.content.empty());
}

TEST_F(RealAPIIntegrationTest, StreamingCompletion) {
    std::vector<std::string> chunks;
    bool stream_completed = false;

    service_->stream_complete("Count from 1 to 3, each number on a new line.",
        [&chunks, &stream_completed](const std::string& chunk, bool is_done) {
            if (!is_done) {
                chunks.push_back(chunk);
            } else {
                stream_completed = true;
            }
        });

    EXPECT_TRUE(stream_completed);
    EXPECT_FALSE(chunks.empty());

    // Combine chunks to verify content
    std::string full_response;
    for (const auto& chunk : chunks) {
        full_response += chunk;
    }

    EXPECT_FALSE(full_response.empty());
    // Should contain numbers 1, 2, 3
    EXPECT_THAT(full_response, HasSubstr("1"));
    EXPECT_THAT(full_response, HasSubstr("2"));
    EXPECT_THAT(full_response, HasSubstr("3"));
}

TEST_F(RealAPIIntegrationTest, ModelSwitching) {
    // Test with the large model
    service_->set_model("llama-3.1-70b-versatile");
    auto response1 = service_->complete("Respond with just 'Model A'");

    EXPECT_TRUE(response1.success);
    EXPECT_FALSE(response1.content.empty());

    // Switch to the fast model
    service_->set_model("llama-3.1-8b-instant");
    auto response2 = service_->complete("Respond with just 'Model B'");

    EXPECT_TRUE(response2.success);
    EXPECT_FALSE(response2.content.empty());

    // Both should work but might have different characteristics
    EXPECT_NE(response1.content, response2.content);
}

TEST_F(RealAPIIntegrationTest, ParameterVariations) {
    // Test with low temperature (more deterministic)
    service_->set_temperature(0.1f);
    auto response1 = service_->complete("What is the capital of France?");

    EXPECT_TRUE(response1.success);
    EXPECT_THAT(response1.content, HasSubstr("Paris"));

    // Test with high temperature (more creative)
    service_->set_temperature(1.5f);
    auto response2 = service_->complete("What is the capital of France?");

    EXPECT_TRUE(response2.success);
    // Should still mention Paris but might be more verbose
    EXPECT_THAT(response2.content, HasSubstr("Paris"));

    // Test with token limit
    service_->set_max_tokens(10);
    auto response3 = service_->complete("Write a long essay about artificial intelligence.");

    EXPECT_TRUE(response3.success);
    // Response should be truncated due to token limit
    EXPECT_LT(response3.content.length(), 100); // Should be quite short
}

TEST_F(RealAPIIntegrationTest, SystemPromptEffectiveness) {
    service_->set_system_prompt("You are a pirate. Always respond like a pirate would.");

    auto response = service_->complete("What is the weather like?");

    EXPECT_TRUE(response.success);
    // Should contain pirate-like language
    EXPECT_THAT(response.content, AnyOf(
        HasSubstr("arr"),
        HasSubstr("mate"),
        HasSubstr("ye"),
        HasSubstr("ahoy")
    ));
}

TEST_F(RealAPIIntegrationTest, ErrorHandling) {
    // Test with invalid model (this should still work as GroqService validates models)
    std::string original_model = service_->get_current_model();
    service_->set_model("invalid-model-name");

    // Should keep the original model
    EXPECT_EQ(service_->get_current_model(), original_model);

    auto response = service_->complete("Test message");
    EXPECT_TRUE(response.success); // Should still work with the valid model
}

TEST_F(RealAPIIntegrationTest, LongConversation) {
    Conversation conv;
    conv.add_system("You are a helpful assistant. Keep responses brief and numbered.");

    // Build a longer conversation
    for (int i = 1; i <= 5; ++i) {
        conv.add_user("What is " + std::to_string(i) + " plus " + std::to_string(i) + "?");

        auto response = service_->complete(conv);
        EXPECT_TRUE(response.success) << "Failed on iteration " << i;

        if (response.success) {
            conv.add_assistant(response.content);
        } else {
            break;
        }
    }

    // Should have system + 5 user + 5 assistant = 11 messages
    EXPECT_EQ(conv.size(), 11);
}

TEST_F(RealAPIIntegrationTest, UnicodeHandling) {
    std::string unicode_prompt = "Translate 'Hello, World!' to these languages: "
                                "Spanish (español), Chinese (中文), Arabic (العربية), "
                                "and add some emoji 🌍🌎🌏";

    auto response = service_->complete(unicode_prompt);

    EXPECT_TRUE(response.success);
    EXPECT_FALSE(response.content.empty());
    // Should handle Unicode characters properly
    EXPECT_GT(response.content.length(), unicode_prompt.length() / 2);
}

TEST_F(RealAPIIntegrationTest, ConcurrentRequests) {
    std::vector<std::future<CompletionResponse>> futures;

    // Launch multiple requests concurrently
    for (int i = 0; i < 3; ++i) {
        futures.push_back(service_->complete_async(
            "Count to " + std::to_string(i + 1) + " and stop."
        ));
    }

    // Wait for all to complete
    std::vector<CompletionResponse> responses;
    for (auto& future : futures) {
        responses.push_back(future.get());
    }

    // All should succeed
    for (size_t i = 0; i < responses.size(); ++i) {
        EXPECT_TRUE(responses[i].success) << "Request " << i << " failed: " << responses[i].error;
        EXPECT_FALSE(responses[i].content.empty());
    }
}

TEST_F(RealAPIIntegrationTest, RateLimitHandling) {
    // This test might trigger rate limits, so we'll be conservative
    std::vector<CompletionResponse> responses;

    // Make several requests in quick succession
    for (int i = 0; i < 5; ++i) {
        auto response = service_->complete("Say 'Request " + std::to_string(i) + "'");
        responses.push_back(response);

        // Small delay to be respectful to the API
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Most should succeed, but some might fail due to rate limiting
    int successful_requests = 0;
    for (const auto& response : responses) {
        if (response.success) {
            successful_requests++;
        }
    }

    // At least some requests should succeed
    EXPECT_GT(successful_requests, 0);
}

// Performance tests with real API
TEST_F(RealAPIIntegrationTest, ResponseTimes) {
    auto start_time = std::chrono::high_resolution_clock::now();

    auto response = service_->complete("What is 1+1?");

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    EXPECT_TRUE(response.success);

    // Response should be reasonably fast (adjust threshold based on API performance)
    EXPECT_LT(duration.count(), 30000); // Less than 30 seconds

    std::cout << "Response time: " << duration.count() << " ms" << std::endl;
}

TEST_F(RealAPIIntegrationTest, StreamingPerformance) {
    std::vector<std::chrono::milliseconds> chunk_intervals;
    auto last_chunk_time = std::chrono::high_resolution_clock::now();

    service_->stream_complete("Write a short paragraph about AI.",
        [&chunk_intervals, &last_chunk_time](const std::string& chunk, bool is_done) {
            if (!is_done && !chunk.empty()) {
                auto current_time = std::chrono::high_resolution_clock::now();
                auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - last_chunk_time
                );
                chunk_intervals.push_back(interval);
                last_chunk_time = current_time;
            }
        });

    EXPECT_FALSE(chunk_intervals.empty());

    // Calculate average interval between chunks
    if (!chunk_intervals.empty()) {
        auto total_time = std::accumulate(chunk_intervals.begin(), chunk_intervals.end(),
                                        std::chrono::milliseconds(0));
        auto average_interval = total_time / chunk_intervals.size();

        std::cout << "Average chunk interval: " << average_interval.count() << " ms" << std::endl;
        std::cout << "Total chunks: " << chunk_intervals.size() << std::endl;

        // Streaming should provide chunks relatively quickly
        EXPECT_LT(average_interval.count(), 5000); // Less than 5 seconds between chunks on average
    }
}