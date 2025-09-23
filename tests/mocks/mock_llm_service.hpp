#pragma once

#include "llm/llm_service.hpp"
#include <gmock/gmock.h>

namespace llm {

class MockLLMService : public LLMService {
public:
    MOCK_METHOD(std::future<CompletionResponse>, complete_async,
                (const Conversation& conversation), (override));

    MOCK_METHOD(CompletionResponse, complete,
                (const Conversation& conversation), (override));

    MOCK_METHOD(CompletionResponse, complete,
                (const std::string& prompt), (override));

    MOCK_METHOD(void, stream_complete,
                (const Conversation& conversation, StreamCallback callback), (override));

    MOCK_METHOD(void, stream_complete,
                (const std::string& prompt, StreamCallback callback), (override));

    MOCK_METHOD(std::vector<ModelInfo>, get_available_models, (), (override));

    MOCK_METHOD(void, set_model, (const std::string& model_id), (override));
    MOCK_METHOD(std::string, get_current_model, (), (const, override));

    MOCK_METHOD(void, set_temperature, (float temperature), (override));
    MOCK_METHOD(void, set_max_tokens, (size_t max_tokens), (override));
    MOCK_METHOD(void, set_system_prompt, (const std::string& prompt), (override));

    MOCK_METHOD(bool, is_available, (), (override));

    // Helper methods for setting up common responses
    void SetupSuccessfulCompletion(const std::string& response_content) {
        CompletionResponse response;
        response.success = true;
        response.content = response_content;
        response.model = "test-model";
        response.tokens_used = 100;

        EXPECT_CALL(*this, complete(testing::_))
            .WillRepeatedly(testing::Return(response));
    }

    void SetupFailedCompletion(const std::string& error_message) {
        CompletionResponse response;
        response.success = false;
        response.error = error_message;

        EXPECT_CALL(*this, complete(testing::_))
            .WillRepeatedly(testing::Return(response));
    }

    void SetupStreamingCompletion(const std::vector<std::string>& chunks) {
        EXPECT_CALL(*this, stream_complete(testing::_, testing::_))
            .WillOnce(testing::Invoke([chunks](const auto&, StreamCallback callback) {
                for (const auto& chunk : chunks) {
                    callback(chunk, false);
                }
                callback("", true); // Signal completion
            }));
    }

    void SetupAvailableModels(const std::vector<ModelInfo>& models) {
        EXPECT_CALL(*this, get_available_models())
            .WillRepeatedly(testing::Return(models));
    }

    void SetupCurrentModel(const std::string& model_name) {
        EXPECT_CALL(*this, get_current_model())
            .WillRepeatedly(testing::Return(model_name));
    }

    void SetupAvailability(bool available) {
        EXPECT_CALL(*this, is_available())
            .WillRepeatedly(testing::Return(available));
    }
};

} // namespace llm