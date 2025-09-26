#include <gtest/gtest.h>
#include "utils/config.hpp"
#include "utils/test_helpers.hpp"

using namespace llm;
using namespace llm::test;

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_ = std::make_unique<Config>();
    }

    std::unique_ptr<Config> config_;
};

TEST_F(ConfigTest, DefaultConfiguration) {
    EXPECT_EQ(config_->get_provider(), "groq");
    EXPECT_TRUE(config_->get_api_key().empty());

    auto repl_config = config_->get_repl_config();
    EXPECT_EQ(repl_config.max_history, 100);
    EXPECT_EQ(repl_config.system_prompt, "You are a helpful AI assistant.");
    EXPECT_TRUE(repl_config.streaming);
    EXPECT_TRUE(repl_config.markdown_rendering);
}

TEST_F(ConfigTest, SetProvider) {
    config_->set_provider("together");
    EXPECT_EQ(config_->get_provider(), "together");

    config_->set_provider("ollama");
    EXPECT_EQ(config_->get_provider(), "ollama");
}

TEST_F(ConfigTest, SetApiKey) {
    std::string test_key = "test-api-key-12345";
    config_->set_api_key(test_key);
    EXPECT_EQ(config_->get_api_key(), test_key);
}

TEST_F(ConfigTest, GetProviderConfig) {
    auto groq_config = config_->get_provider_config("groq");
    EXPECT_EQ(groq_config.model, "llama-3.1-70b-versatile");
    EXPECT_EQ(groq_config.api_url, "https://api.groq.com/openai/v1");
    EXPECT_EQ(groq_config.temperature, 0.7f);
    EXPECT_EQ(groq_config.max_tokens, 2048);

    auto together_config = config_->get_provider_config("together");
    EXPECT_EQ(together_config.model, "meta-llama/Llama-2-70b-chat-hf");
    EXPECT_EQ(together_config.api_url, "https://api.together.xyz/v1");

    auto ollama_config = config_->get_provider_config("ollama");
    EXPECT_EQ(ollama_config.model, "llama3.1");
    EXPECT_EQ(ollama_config.api_url, "http://localhost:11434");
}

TEST_F(ConfigTest, SetProviderConfig) {
    ProviderConfig custom_config;
    custom_config.model = "custom-model";
    custom_config.temperature = 0.9f;
    custom_config.max_tokens = 4096;
    custom_config.api_url = "https://custom.api.com";

    config_->set_provider_config("custom", custom_config);

    auto retrieved_config = config_->get_provider_config("custom");
    EXPECT_EQ(retrieved_config.model, "custom-model");
    EXPECT_EQ(retrieved_config.temperature, 0.9f);
    EXPECT_EQ(retrieved_config.max_tokens, 4096);
    EXPECT_EQ(retrieved_config.api_url, "https://custom.api.com");
}

TEST_F(ConfigTest, SetReplConfig) {
    ReplConfig custom_repl;
    custom_repl.history_file = "/custom/history";
    custom_repl.max_history = 200;
    custom_repl.system_prompt = "Custom system prompt";
    custom_repl.streaming = false;
    custom_repl.markdown_rendering = false;
    custom_repl.prompt_prefix = ">> ";
    custom_repl.ai_prefix = "Bot: ";

    config_->set_repl_config(custom_repl);

    auto retrieved_config = config_->get_repl_config();
    EXPECT_EQ(retrieved_config.history_file, "/custom/history");
    EXPECT_EQ(retrieved_config.max_history, 200);
    EXPECT_EQ(retrieved_config.system_prompt, "Custom system prompt");
    EXPECT_FALSE(retrieved_config.streaming);
    EXPECT_FALSE(retrieved_config.markdown_rendering);
    EXPECT_EQ(retrieved_config.prompt_prefix, ">> ");
    EXPECT_EQ(retrieved_config.ai_prefix, "Bot: ");
}

TEST_F(ConfigTest, ToJsonSerialization) {
    config_->set_provider("groq");
    config_->set_api_key("test-key");

    auto json = config_->to_json();

    EXPECT_EQ(json["provider"], "groq");
    EXPECT_EQ(json["api_key"], "test-key");
    EXPECT_TRUE(json.contains("groq"));
    EXPECT_TRUE(json.contains("together"));
    EXPECT_TRUE(json.contains("ollama"));
    EXPECT_TRUE(json.contains("repl"));

    EXPECT_EQ(json["groq"]["model"], "llama-3.1-70b-versatile");
    EXPECT_EQ(json["repl"]["streaming"], true);
}

TEST_F(ConfigTest, FromJsonDeserialization) {
    nlohmann::json config_json = {
        {"provider", "together"},
        {"api_key", "test-api-key"},
        {"groq", {
            {"model", "custom-groq-model"},
            {"temperature", 0.8},
            {"max_tokens", 1024},
            {"api_url", "https://custom-groq.com"}
        }},
        {"repl", {
            {"history_file", "/custom/history"},
            {"max_history", 150},
            {"system_prompt", "Custom prompt"},
            {"streaming", false}
        }}
    };

    config_->from_json(config_json);

    EXPECT_EQ(config_->get_provider(), "together");
    EXPECT_EQ(config_->get_api_key(), "test-api-key");

    auto groq_config = config_->get_provider_config("groq");
    EXPECT_EQ(groq_config.model, "custom-groq-model");
    EXPECT_EQ(groq_config.temperature, 0.8f);
    EXPECT_EQ(groq_config.max_tokens, 1024);

    auto repl_config = config_->get_repl_config();
    EXPECT_EQ(repl_config.history_file, "/custom/history");
    EXPECT_EQ(repl_config.max_history, 150);
    EXPECT_EQ(repl_config.system_prompt, "Custom prompt");
    EXPECT_FALSE(repl_config.streaming);
}

TEST_F(ConfigTest, RoundTripJsonConversion) {
    config_->set_provider("ollama");
    config_->set_api_key("test-key");

    ProviderConfig custom_provider;
    custom_provider.model = "test-model";
    custom_provider.temperature = 0.5f;
    config_->set_provider_config("test", custom_provider);

    auto json = config_->to_json();

    Config new_config;
    new_config.from_json(json);

    EXPECT_EQ(new_config.get_provider(), config_->get_provider());
    EXPECT_EQ(new_config.get_api_key(), config_->get_api_key());

    auto original_test_config = config_->get_provider_config("test");
    auto new_test_config = new_config.get_provider_config("test");
    EXPECT_EQ(new_test_config.model, original_test_config.model);
    EXPECT_EQ(new_test_config.temperature, original_test_config.temperature);
}

TEST_F(ConfigTest, LoadFromFile) {
    TempFile config_file(R"({
        "provider": "together",
        "api_key": "file-api-key",
        "groq": {
            "model": "file-model",
            "temperature": 0.3
        },
        "repl": {
            "max_history": 75,
            "streaming": false
        }
    })");

    bool loaded = config_->load_from_file(config_file.path());

    EXPECT_TRUE(loaded);
    EXPECT_EQ(config_->get_provider(), "together");
    EXPECT_EQ(config_->get_api_key(), "file-api-key");

    auto groq_config = config_->get_provider_config("groq");
    EXPECT_EQ(groq_config.model, "file-model");
    EXPECT_EQ(groq_config.temperature, 0.3f);

    auto repl_config = config_->get_repl_config();
    EXPECT_EQ(repl_config.max_history, 75);
    EXPECT_FALSE(repl_config.streaming);
}

TEST_F(ConfigTest, LoadFromNonexistentFile) {
    bool loaded = config_->load_from_file("/nonexistent/file.json");
    EXPECT_FALSE(loaded);

    // Should still have default values
    EXPECT_EQ(config_->get_provider(), "groq");
}

TEST_F(ConfigTest, SaveToFile) {
    config_->set_provider("together");
    config_->set_api_key("save-test-key");

    TempFile config_file;
    bool saved = config_->save_to_file(config_file.path());

    EXPECT_TRUE(saved);

    // Load the saved file and verify
    Config loaded_config;
    loaded_config.load_from_file(config_file.path());

    EXPECT_EQ(loaded_config.get_provider(), "together");
    EXPECT_EQ(loaded_config.get_api_key(), "save-test-key");
}

TEST_F(ConfigTest, MergeCommandLineArgs) {
    std::map<std::string, std::string> args = {
        {"provider", "ollama"},
        {"model", "custom-cli-model"},
        {"api-key", "cli-api-key"},
        {"temperature", "0.9"}
    };

    config_->merge_command_line_args(args);

    EXPECT_EQ(config_->get_provider(), "ollama");
    EXPECT_EQ(config_->get_api_key(), "cli-api-key");

    auto provider_config = config_->get_provider_config("ollama");
    EXPECT_EQ(provider_config.model, "custom-cli-model");
    EXPECT_EQ(provider_config.temperature, 0.9f);
}

TEST_F(ConfigTest, ExpandPath) {
    EXPECT_EQ(config_->expand_path("relative/path"),
              std::filesystem::absolute("relative/path").string());

    EXPECT_EQ(config_->expand_path("/absolute/path"),
              std::filesystem::absolute("/absolute/path").string());

    // Test tilde expansion (platform dependent)
    std::string tilde_path = config_->expand_path("~/test");
    EXPECT_NE(tilde_path, "~/test"); // Should be expanded
    EXPECT_THAT(tilde_path, Not(testing::HasSubstr("~")));
}

TEST_F(ConfigTest, EnvironmentVariableIntegration) {
    // Test environment variable support
    EnvVar test_env("GROQ_API_KEY", "env-api-key");

    config_->set_from_environment();

    EXPECT_EQ(config_->get_api_key(), "env-api-key");
}

TEST_F(ConfigTest, ProviderEnvironmentVariable) {
    EnvVar provider_env("LLM_PROVIDER", "together");

    config_->set_from_environment();

    EXPECT_EQ(config_->get_provider(), "together");
}

TEST_F(ConfigTest, InvalidTemperatureInArgs) {
    std::map<std::string, std::string> args = {
        {"temperature", "invalid_number"}
    };

    // Should not crash on invalid temperature
    EXPECT_NO_THROW(config_->merge_command_line_args(args));

    // Should keep default temperature
    auto provider_config = config_->get_provider_config(config_->get_provider());
    EXPECT_EQ(provider_config.temperature, 0.7f);
}