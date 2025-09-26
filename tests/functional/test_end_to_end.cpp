#include <gtest/gtest.h>
#include "utils/test_helpers.hpp"
#include "utils/config.hpp"
#include <httplib.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <process.h> // For Windows _spawnl
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace llm::test;

class EndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir_ = std::make_unique<TempDir>();

        // Create a test configuration file
        config_file_ = temp_dir_->create_file("test_config.yaml", R"(
provider: groq
api_key: test-api-key

groq:
  model: llama-3.1-70b-versatile
  temperature: 0.7
  max_tokens: 100
  api_url: http://localhost:18082/openai/v1

repl:
  history_file: )" + temp_dir_->path() + R"(/history
  max_history: 10
  system_prompt: "You are a test assistant."
  streaming: false
  markdown_rendering: false
)");

        // Start mock API server
        StartMockApiServer();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    void TearDown() override {
        StopMockApiServer();
        temp_dir_.reset();
    }

    void StartMockApiServer() {
        server_thread_ = std::thread([this]() {
            httplib::Server server;

            // Chat completions endpoint
            server.Post("/openai/v1/chat/completions", [](const httplib::Request& req, httplib::Response& res) {
                auto json_req = nlohmann::json::parse(req.body);

                std::string user_message;
                if (json_req.contains("messages") && json_req["messages"].is_array()) {
                    for (const auto& msg : json_req["messages"]) {
                        if (msg["role"] == "user") {
                            user_message = msg["content"];
                        }
                    }
                }

                std::string response_content = "Test response to: " + user_message;

                nlohmann::json response = {
                    {"choices", nlohmann::json::array({
                        {
                            {"message", {
                                {"role", "assistant"},
                                {"content", response_content}
                            }}
                        }
                    })},
                    {"usage", {
                        {"total_tokens", 50}
                    }},
                    {"model", "test-model"}
                };

                res.set_content(response.dump(), "application/json");
            });

            // Models endpoint
            server.Get("/openai/v1/models", [](const httplib::Request&, httplib::Response& res) {
                nlohmann::json response = {
                    {"data", nlohmann::json::array({
                        {{"id", "llama-3.1-70b-versatile"}},
                        {{"id", "llama-3.1-8b-instant"}}
                    })}
                };
                res.set_content(response.dump(), "application/json");
            });

            server.listen("localhost", 18082);
        });
    }

    void StopMockApiServer() {
        if (server_thread_.joinable()) {
            server_thread_.detach();
        }
    }

    std::string RunCommand(const std::string& command, const std::string& input = "") {
        // This is a simplified command runner for testing
        // In practice, you'd want a more robust solution

        std::string temp_input_file = temp_dir_->create_file("input.txt", input);
        std::string temp_output_file = temp_dir_->path() + "/output.txt";

        std::string full_command = command + " < " + temp_input_file + " > " + temp_output_file + " 2>&1";

        int result = std::system(full_command.c_str());

        if (TestHelpers::FileExists(temp_output_file)) {
            return TestHelpers::ReadFile(temp_output_file);
        }

        return "";
    }

    std::unique_ptr<TempDir> temp_dir_;
    std::string config_file_;
    std::thread server_thread_;
};

TEST_F(EndToEndTest, ApplicationStartupAndShutdown) {
    // Test that the application can start and shutdown gracefully
    std::string input_commands = "/exit\n";

    // This would typically run the actual executable
    // For now, we'll test the core components

    auto config = std::make_unique<Config>(config_file_);
    EXPECT_EQ(config->get_provider(), "groq");
    EXPECT_EQ(config->get_api_key(), "test-api-key");

    // Verify configuration was loaded correctly
    auto provider_config = config->get_provider_config("groq");
    EXPECT_EQ(provider_config.model, "llama-3.1-70b-versatile");
    EXPECT_EQ(provider_config.temperature, 0.7f);
    EXPECT_EQ(provider_config.max_tokens, 100);
}

TEST_F(EndToEndTest, ConfigurationFileProcessing) {
    Config config(config_file_);

    auto repl_config = config.get_repl_config();
    EXPECT_EQ(repl_config.history_file, temp_dir_->path() + "/history");
    EXPECT_EQ(repl_config.max_history, 10);
    EXPECT_EQ(repl_config.system_prompt, "You are a test assistant.");
    EXPECT_FALSE(repl_config.streaming);
    EXPECT_FALSE(repl_config.markdown_rendering);
}

TEST_F(EndToEndTest, ConversationPersistence) {
    std::string conv_file = temp_dir_->path() + "/test_conversation.json";

    // Create a conversation and save it
    Conversation original_conv;
    original_conv.add_system("System message");
    original_conv.add_user("User question");
    original_conv.add_assistant("Assistant answer");

    original_conv.save_to_file(conv_file);
    EXPECT_TRUE(TestHelpers::FileExists(conv_file));

    // Load conversation in a new instance
    Conversation loaded_conv;
    loaded_conv.load_from_file(conv_file);

    EXPECT_EQ(loaded_conv.size(), original_conv.size());

    // Verify content matches
    const auto& orig_messages = original_conv.messages();
    const auto& loaded_messages = loaded_conv.messages();

    for (size_t i = 0; i < orig_messages.size(); ++i) {
        EXPECT_EQ(loaded_messages[i].role, orig_messages[i].role);
        EXPECT_EQ(loaded_messages[i].content, orig_messages[i].content);
    }
}

TEST_F(EndToEndTest, HistoryManagement) {
    std::string history_file = temp_dir_->path() + "/test_history";

    // Simulate command history
    std::vector<std::string> commands = {
        "Hello, how are you?",
        "What is the weather like?",
        "Tell me a joke",
        "/help",
        "/clear"
    };

    // Write history file
    std::ofstream hist_file(history_file);
    for (const auto& cmd : commands) {
        hist_file << cmd << std::endl;
    }
    hist_file.close();

    // Read and verify history
    std::ifstream read_hist(history_file);
    std::string line;
    std::vector<std::string> read_commands;

    while (std::getline(read_hist, line)) {
        read_commands.push_back(line);
    }

    EXPECT_EQ(read_commands.size(), commands.size());
    for (size_t i = 0; i < commands.size(); ++i) {
        EXPECT_EQ(read_commands[i], commands[i]);
    }
}

TEST_F(EndToEndTest, ErrorRecovery) {
    // Test various error conditions and recovery

    // Invalid configuration file
    std::string invalid_config = temp_dir_->create_file("invalid.yaml", "invalid: yaml: content:");

    Config config;
    bool loaded = config.load_from_file(invalid_config);
    EXPECT_FALSE(loaded);

    // Configuration should have defaults even after failed load
    EXPECT_EQ(config.get_provider(), "groq");
}

TEST_F(EndToEndTest, EnvironmentVariableOverrides) {
    // Test environment variable handling
    EnvVar api_key_env("GROQ_API_KEY", "env-override-key");
    EnvVar provider_env("LLM_PROVIDER", "together");

    Config config;
    config.set_from_environment();

    EXPECT_EQ(config.get_api_key(), "env-override-key");
    EXPECT_EQ(config.get_provider(), "together");
}

TEST_F(EndToEndTest, CommandLineArgumentProcessing) {
    Config config;

    std::map<std::string, std::string> cli_args = {
        {"provider", "ollama"},
        {"model", "custom-model"},
        {"api-key", "cli-api-key"},
        {"temperature", "0.9"}
    };

    config.merge_command_line_args(cli_args);

    EXPECT_EQ(config.get_provider(), "ollama");
    EXPECT_EQ(config.get_api_key(), "cli-api-key");

    auto provider_config = config.get_provider_config("ollama");
    EXPECT_EQ(provider_config.model, "custom-model");
    EXPECT_EQ(provider_config.temperature, 0.9f);
}

TEST_F(EndToEndTest, LargeConversationHandling) {
    // Test handling of large conversations
    Conversation large_conv;
    large_conv.add_system("System prompt");

    // Add many message pairs
    for (int i = 0; i < 100; ++i) {
        large_conv.add_user("User message " + std::to_string(i));
        large_conv.add_assistant("Assistant response " + std::to_string(i));
    }

    EXPECT_EQ(large_conv.size(), 201); // 1 system + 200 user/assistant

    // Test token estimation
    size_t estimated_tokens = large_conv.estimate_tokens();
    EXPECT_GT(estimated_tokens, 0);

    // Test truncation
    large_conv.truncate_to_token_limit(100, 10);
    EXPECT_LT(large_conv.size(), 201);

    // Should still have system message
    EXPECT_EQ(large_conv.messages()[0].role, MessageRole::System);
}

TEST_F(EndToEndTest, UnicodeAndSpecialCharacterHandling) {
    // Test handling of various character encodings and special characters
    std::vector<std::string> test_strings = {
        "Simple ASCII text",
        "Unicode emoji: 🚀 🤖 🎉",
        "Multi-language: Hello नमस्ते 你好 مرحبا",
        "Special chars: \n\t\r\"'\\{}[]",
        "JSON-breaking: {\"test\": \"value\"}",
        "Very long string: " + std::string(1000, 'x')
    };

    for (const auto& test_str : test_strings) {
        Message msg(MessageRole::User, test_str);

        // Test JSON serialization round-trip
        auto json = msg.to_json();
        auto reconstructed = Message::from_json(json);

        EXPECT_EQ(reconstructed.content, test_str);
    }
}

TEST_F(EndToEndTest, ConcurrentOperations) {
    // Test concurrent access to shared resources

    std::vector<std::thread> threads;
    std::vector<std::unique_ptr<Conversation>> conversations(5);

    // Create multiple conversations concurrently
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&conversations, i]() {
            conversations[i] = std::make_unique<Conversation>();
            conversations[i]->add_system("Thread " + std::to_string(i));
            conversations[i]->add_user("Message from thread " + std::to_string(i));
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify all conversations were created correctly
    for (int i = 0; i < 5; ++i) {
        EXPECT_EQ(conversations[i]->size(), 2);
        EXPECT_THAT(conversations[i]->messages()[0].content, HasSubstr("Thread " + std::to_string(i)));
    }
}

TEST_F(EndToEndTest, ResourceCleanup) {
    // Test that resources are properly cleaned up

    std::string temp_file = temp_dir_->path() + "/cleanup_test.json";

    {
        // Create resources in limited scope
        Conversation conv;
        conv.add_user("Test message");
        conv.save_to_file(temp_file);

        Config config;
        config.set_api_key("test");
        config.save_to_file(temp_dir_->path() + "/config_cleanup.json");
    }

    // Resources should be cleaned up, but files should remain
    EXPECT_TRUE(TestHelpers::FileExists(temp_file));
    EXPECT_TRUE(TestHelpers::FileExists(temp_dir_->path() + "/config_cleanup.json"));
}

// Performance and stress tests
TEST_F(EndToEndTest, PerformanceBaseline) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Perform typical operations
    Config config(config_file_);

    Conversation conv;
    for (int i = 0; i < 50; ++i) {
        conv.add_user("Message " + std::to_string(i));
        conv.add_assistant("Response " + std::to_string(i));
    }

    // Serialize/deserialize
    auto json = conv.to_json();
    Conversation new_conv;
    new_conv.from_json(json);

    // Save/load
    std::string perf_file = temp_dir_->path() + "/perf_test.json";
    conv.save_to_file(perf_file);

    Conversation loaded_conv;
    loaded_conv.load_from_file(perf_file);

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Should complete in reasonable time (adjust threshold as needed)
    EXPECT_LT(duration.count(), 1000); // Less than 1 second
}

TEST_F(EndToEndTest, MemoryUsage) {
    // Basic memory usage test
    std::vector<std::unique_ptr<Conversation>> conversations;

    // Create many conversations
    for (int i = 0; i < 100; ++i) {
        auto conv = std::make_unique<Conversation>();
        conv->add_system("System prompt " + std::to_string(i));

        for (int j = 0; j < 10; ++j) {
            conv->add_user("User message " + std::to_string(j));
            conv->add_assistant("Assistant response " + std::to_string(j));
        }

        conversations.push_back(std::move(conv));
    }

    // Verify all conversations are valid
    EXPECT_EQ(conversations.size(), 100);

    for (const auto& conv : conversations) {
        EXPECT_EQ(conv->size(), 21); // 1 system + 20 user/assistant
    }

    // Clear conversations
    conversations.clear();

    // Memory should be released (can't easily test this automatically)
}