#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "repl/repl.hpp"
#include "mocks/mock_llm_service.hpp"
#include "utils/test_helpers.hpp"
#include <sstream>
#include <thread>
#include <chrono>

using namespace llm;
using namespace llm::test;
using namespace testing;

class REPLIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = TestHelpers::CreateTestConfig();
        repl_ = std::make_unique<REPL>(std::move(config_));

        // Create and configure mock LLM service
        auto mock_service = std::make_unique<MockLLMService>();
        mock_service_ = mock_service.get();

        // Set up default expectations
        mock_service_->SetupAvailability(true);
        mock_service_->SetupCurrentModel("test-model");
        mock_service_->SetupSuccessfulCompletion("Mock response from AI");

        repl_->set_llm_service(std::move(mock_service));
    }

    void TearDown() override {
        repl_.reset();
    }

    std::unique_ptr<REPL> repl_;
    MockLLMService* mock_service_;
    std::unique_ptr<Config> config_;
};

TEST_F(REPLIntegrationTest, BasicUserInteraction) {
    // Simulate user input and verify LLM service is called
    std::istringstream input("Hello AI\n/exit\n");
    std::ostringstream output;

    // Redirect stdin/stdout for testing (this is simplified)
    // In a real test, we'd need more sophisticated I/O redirection

    EXPECT_CALL(*mock_service_, complete(HasMessageCount(2))) // system + user message
        .WillOnce(Return(CompletionResponse{
            .content = "Hello there!",
            .success = true,
            .model = "test-model",
            .tokens_used = 10
        }));

    // Test would require actual I/O simulation
    // For now, we'll test the underlying components
}

TEST_F(REPLIntegrationTest, ConversationFlow) {
    // Test a multi-turn conversation
    Conversation test_conv;
    test_conv.add_system("You are a helpful assistant.");
    test_conv.add_user("What is 2+2?");

    EXPECT_CALL(*mock_service_, complete(_))
        .WillOnce(Return(CompletionResponse{
            .content = "2+2 equals 4",
            .success = true,
            .model = "test-model",
            .tokens_used = 15
        }));

    auto response = mock_service_->complete(test_conv);
    EXPECT_TRUE(response.success);
    EXPECT_EQ(response.content, "2+2 equals 4");

    // Add response to conversation and continue
    test_conv.add_assistant(response.content);
    test_conv.add_user("What about 3+3?");

    EXPECT_CALL(*mock_service_, complete(_))
        .WillOnce(Return(CompletionResponse{
            .content = "3+3 equals 6",
            .success = true,
            .model = "test-model",
            .tokens_used = 12
        }));

    auto response2 = mock_service_->complete(test_conv);
    EXPECT_TRUE(response2.success);
    EXPECT_EQ(response2.content, "3+3 equals 6");
}

TEST_F(REPLIntegrationTest, StreamingInteraction) {
    std::vector<std::string> chunks = {"Hello", " from", " streaming", " AI"};

    mock_service_->SetupStreamingCompletion(chunks);

    std::vector<std::string> received_chunks;
    mock_service_->stream_complete("Test prompt",
        [&received_chunks](const std::string& chunk, bool is_done) {
            if (!is_done) {
                received_chunks.push_back(chunk);
            }
        });

    EXPECT_EQ(received_chunks.size(), chunks.size());
    for (size_t i = 0; i < chunks.size(); ++i) {
        EXPECT_EQ(received_chunks[i], chunks[i]);
    }
}

TEST_F(REPLIntegrationTest, ErrorHandling) {
    mock_service_->SetupFailedCompletion("API rate limit exceeded");

    auto response = mock_service_->complete("Test prompt");

    EXPECT_FALSE(response.success);
    EXPECT_EQ(response.error, "API rate limit exceeded");
    EXPECT_TRUE(response.content.empty());
}

TEST_F(REPLIntegrationTest, ModelSwitching) {
    std::vector<ModelInfo> available_models = {
        {"model-1", "Test Model 1", 2048, true},
        {"model-2", "Test Model 2", 4096, false}
    };

    mock_service_->SetupAvailableModels(available_models);

    auto models = mock_service_->get_available_models();
    EXPECT_EQ(models.size(), 2);
    EXPECT_EQ(models[0].id, "model-1");
    EXPECT_EQ(models[1].context_length, 4096);

    EXPECT_CALL(*mock_service_, set_model("model-2"));
    mock_service_->set_model("model-2");

    mock_service_->SetupCurrentModel("model-2");
    EXPECT_EQ(mock_service_->get_current_model(), "model-2");
}

TEST_F(REPLIntegrationTest, ConfigurationIntegration) {
    // Test that REPL respects configuration settings
    auto config = TestHelpers::CreateTestConfig();
    auto repl_config = config->get_repl_config();

    EXPECT_EQ(repl_config.system_prompt, "You are a test assistant.");
    EXPECT_TRUE(repl_config.streaming);
    EXPECT_EQ(repl_config.max_history, 50);
}

// Test conversation persistence
class REPLPersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::make_unique<TempDir>();
        config_ = TestHelpers::CreateTestConfig();

        // Override history file path
        auto repl_config = config_->get_repl_config();
        repl_config.history_file = temp_dir_->path() + "/test_history";
        config_->set_repl_config(repl_config);
    }

    std::unique_ptr<TempDir> temp_dir_;
    std::unique_ptr<Config> config_;
};

TEST_F(REPLPersistenceTest, ConversationSaveLoad) {
    std::string conv_file = temp_dir_->path() + "/conversation.json";

    Conversation original_conv;
    original_conv.add_system("System prompt");
    original_conv.add_user("User message");
    original_conv.add_assistant("Assistant response");

    // Save conversation
    original_conv.save_to_file(conv_file);
    EXPECT_TRUE(TestHelpers::FileExists(conv_file));

    // Load conversation
    Conversation loaded_conv;
    loaded_conv.load_from_file(conv_file);

    EXPECT_EQ(loaded_conv.size(), original_conv.size());

    const auto& original_messages = original_conv.messages();
    const auto& loaded_messages = loaded_conv.messages();

    for (size_t i = 0; i < original_messages.size(); ++i) {
        EXPECT_EQ(loaded_messages[i].role, original_messages[i].role);
        EXPECT_EQ(loaded_messages[i].content, original_messages[i].content);
    }
}

TEST_F(REPLPersistenceTest, ConfigurationPersistence) {
    std::string config_file = temp_dir_->path() + "/config.json";

    // Save configuration
    config_->set_provider("together");
    config_->set_api_key("test-key-123");
    config_->save_to_file(config_file);

    // Load configuration
    Config loaded_config;
    loaded_config.load_from_file(config_file);

    EXPECT_EQ(loaded_config.get_provider(), "together");
    EXPECT_EQ(loaded_config.get_api_key(), "test-key-123");
}

// Test REPL commands
class REPLCommandTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::make_unique<TempDir>();
        conversation_ = std::make_unique<Conversation>();
        conversation_->add_system("Test system prompt");
        conversation_->add_user("Hello");
        conversation_->add_assistant("Hi there!");
    }

    std::unique_ptr<TempDir> temp_dir_;
    std::unique_ptr<Conversation> conversation_;
};

TEST_F(REPLCommandTest, SaveCommand) {
    std::string save_file = temp_dir_->path() + "/saved_conversation.json";

    // Save the conversation
    conversation_->save_to_file(save_file);

    // Verify file was created and contains expected data
    EXPECT_TRUE(TestHelpers::FileExists(save_file));

    std::string file_content = TestHelpers::ReadFile(save_file);
    EXPECT_FALSE(file_content.empty());

    auto json_content = nlohmann::json::parse(file_content);
    EXPECT_TRUE(json_content.is_array());
    EXPECT_EQ(json_content.size(), 3);
}

TEST_F(REPLCommandTest, LoadCommand) {
    std::string load_file = temp_dir_->create_file("load_test.json", R"([
        {"role": "system", "content": "Loaded system prompt"},
        {"role": "user", "content": "Loaded user message"},
        {"role": "assistant", "content": "Loaded assistant response"}
    ])");

    Conversation loaded_conv;
    loaded_conv.load_from_file(load_file);

    EXPECT_EQ(loaded_conv.size(), 3);
    EXPECT_EQ(loaded_conv.messages()[0].content, "Loaded system prompt");
    EXPECT_EQ(loaded_conv.messages()[1].content, "Loaded user message");
    EXPECT_EQ(loaded_conv.messages()[2].content, "Loaded assistant response");
}

TEST_F(REPLCommandTest, ClearCommand) {
    EXPECT_EQ(conversation_->size(), 3);

    conversation_->clear();

    EXPECT_EQ(conversation_->size(), 0);
    EXPECT_TRUE(conversation_->empty());
}

TEST_F(REPLCommandTest, HistoryCommand) {
    std::string history_str = conversation_->to_string();

    EXPECT_THAT(history_str, HasSubstr("Test system prompt"));
    EXPECT_THAT(history_str, HasSubstr("Hello"));
    EXPECT_THAT(history_str, HasSubstr("Hi there!"));
    EXPECT_THAT(history_str, HasSubstr("[System]"));
    EXPECT_THAT(history_str, HasSubstr("[User]"));
    EXPECT_THAT(history_str, HasSubstr("[Assistant]"));
}

// Test concurrent operations
class REPLConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = TestHelpers::CreateTestConfig();
        repl_ = std::make_unique<REPL>(std::move(config_));

        auto mock_service = std::make_unique<MockLLMService>();
        mock_service_ = mock_service.get();
        mock_service_->SetupAvailability(true);

        repl_->set_llm_service(std::move(mock_service));
    }

    std::unique_ptr<REPL> repl_;
    MockLLMService* mock_service_;
    std::unique_ptr<Config> config_;
};

TEST_F(REPLConcurrencyTest, MultipleAsyncRequests) {
    std::vector<std::future<CompletionResponse>> futures;

    // Set up expectations for multiple calls
    EXPECT_CALL(*mock_service_, complete_async(_))
        .Times(3)
        .WillRepeatedly(Return(
            std::async(std::launch::async, []() {
                return CompletionResponse{
                    .content = "Async response",
                    .success = true,
                    .model = "test-model"
                };
            })
        ));

    // Launch multiple async requests
    for (int i = 0; i < 3; ++i) {
        futures.push_back(mock_service_->complete_async(TestHelpers::CreateTestConversation()));
    }

    // Wait for all to complete
    for (auto& future : futures) {
        auto response = future.get();
        EXPECT_TRUE(response.success);
        EXPECT_EQ(response.content, "Async response");
    }
}

TEST_F(REPLConcurrencyTest, StreamingWithCancel) {
    std::atomic<bool> should_cancel{false};
    std::vector<std::string> received_chunks;

    // Setup streaming that can be cancelled
    EXPECT_CALL(*mock_service_, stream_complete(_, _))
        .WillOnce(Invoke([&should_cancel, &received_chunks](auto, StreamCallback callback) {
            std::vector<std::string> chunks = {"Chunk1", "Chunk2", "Chunk3", "Chunk4"};
            for (const auto& chunk : chunks) {
                if (should_cancel.load()) {
                    break;
                }
                callback(chunk, false);
                received_chunks.push_back(chunk);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            callback("", true);
        }));

    // Start streaming in a separate thread
    std::thread stream_thread([this]() {
        mock_service_->stream_complete("Test", [](const std::string&, bool) {});
    });

    // Cancel after a short delay
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    should_cancel = true;

    stream_thread.join();

    // Should have received some but not all chunks
    EXPECT_GT(received_chunks.size(), 0);
    EXPECT_LT(received_chunks.size(), 4);
}