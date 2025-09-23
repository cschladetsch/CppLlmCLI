#include "test_helpers.hpp"
#include <fstream>
#include <random>
#include <algorithm>
#include <cstdlib>

namespace llm {
namespace test {

std::vector<std::string> TestHelpers::temp_paths_;

std::unique_ptr<Config> TestHelpers::CreateTestConfig() {
    auto config = std::make_unique<Config>();

    // Set up test provider configurations
    ProviderConfig groq_config;
    groq_config.model = "test-model";
    groq_config.temperature = 0.7f;
    groq_config.max_tokens = 1024;
    groq_config.api_url = "http://localhost:8080/test";
    config->set_provider_config("groq", groq_config);

    config->set_provider("groq");
    config->set_api_key("test-api-key");

    ReplConfig repl_config;
    repl_config.history_file = CreateTempFile();
    repl_config.max_history = 50;
    repl_config.system_prompt = "You are a test assistant.";
    repl_config.streaming = true;
    config->set_repl_config(repl_config);

    return config;
}

Conversation TestHelpers::CreateTestConversation() {
    Conversation conv;
    conv.add_system("You are a helpful test assistant.");
    conv.add_user("Hello, how are you?");
    conv.add_assistant("I'm doing well, thank you for asking!");
    conv.add_user("What can you help me with?");
    return conv;
}

std::string TestHelpers::CreateTempFile(const std::string& content) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);

    std::string filename = "/tmp/test_" + std::to_string(dis(gen)) + ".tmp";

    if (!content.empty()) {
        WriteFile(filename, content);
    }

    temp_paths_.push_back(filename);
    return filename;
}

std::string TestHelpers::CreateTempDir() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100000, 999999);

    std::string dirname = "/tmp/test_dir_" + std::to_string(dis(gen));
    std::filesystem::create_directories(dirname);

    temp_paths_.push_back(dirname);
    return dirname;
}

void TestHelpers::CleanupTempPath(const std::string& path) {
    try {
        if (std::filesystem::exists(path)) {
            std::filesystem::remove_all(path);
        }

        auto it = std::find(temp_paths_.begin(), temp_paths_.end(), path);
        if (it != temp_paths_.end()) {
            temp_paths_.erase(it);
        }
    } catch (const std::exception&) {
        // Ignore cleanup errors in tests
    }
}

bool TestHelpers::JsonEquals(const nlohmann::json& a, const nlohmann::json& b) {
    return a == b;
}

std::string TestHelpers::TrimWhitespace(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";

    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> TestHelpers::SplitString(const std::string& str, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = str.find(delimiter);

    while (end != std::string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + delimiter.length();
        end = str.find(delimiter, start);
    }

    tokens.push_back(str.substr(start));
    return tokens;
}

std::string TestHelpers::ReadFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return "";

    std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
    return content;
}

void TestHelpers::WriteFile(const std::string& filepath, const std::string& content) {
    std::ofstream file(filepath);
    file << content;
}

bool TestHelpers::FileExists(const std::string& filepath) {
    return std::filesystem::exists(filepath);
}

std::vector<Message> TestHelpers::GenerateTestMessages(size_t count) {
    std::vector<Message> messages;

    for (size_t i = 0; i < count; ++i) {
        MessageRole role = (i % 2 == 0) ? MessageRole::User : MessageRole::Assistant;
        std::string content = "Test message " + std::to_string(i + 1);
        messages.emplace_back(role, content);
    }

    return messages;
}

nlohmann::json TestHelpers::GenerateTestApiResponse(const std::string& content) {
    return nlohmann::json{
        {"choices", nlohmann::json::array({
            {
                {"message", {
                    {"role", "assistant"},
                    {"content", content}
                }}
            }
        })},
        {"usage", {
            {"total_tokens", 100},
            {"prompt_tokens", 50},
            {"completion_tokens", 50}
        }},
        {"model", "test-model"}
    };
}

nlohmann::json TestHelpers::GenerateTestErrorResponse(const std::string& error) {
    return nlohmann::json{
        {"error", {
            {"message", error},
            {"type", "test_error"},
            {"code", "test_code"}
        }}
    };
}

std::string TestHelpers::CreateMockGroqResponse(const std::string& content, size_t tokens) {
    auto json_response = nlohmann::json{
        {"choices", nlohmann::json::array({
            {
                {"message", {
                    {"role", "assistant"},
                    {"content", content}
                }}
            }
        })},
        {"usage", {
            {"total_tokens", tokens}
        }},
        {"model", "test-model"}
    };

    return json_response.dump();
}

std::string TestHelpers::CreateMockStreamingChunk(const std::string& content) {
    auto chunk_data = nlohmann::json{
        {"choices", nlohmann::json::array({
            {
                {"delta", {
                    {"content", content}
                }}
            }
        })}
    };

    return "data: " + chunk_data.dump() + "\n\n";
}

std::string TestHelpers::CreateMockModelsResponse() {
    auto models_response = nlohmann::json{
        {"data", nlohmann::json::array({
            {
                {"id", "test-model-1"},
                {"object", "model"},
                {"created", 1234567890}
            },
            {
                {"id", "test-model-2"},
                {"object", "model"},
                {"created", 1234567891}
            }
        })}
    };

    return models_response.dump();
}

void TestHelpers::SetEnvironmentVariable(const std::string& name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
}

void TestHelpers::UnsetEnvironmentVariable(const std::string& name) {
#ifdef _WIN32
    _putenv_s(name.c_str(), "");
#else
    unsetenv(name.c_str());
#endif
}

std::string TestHelpers::GetEnvironmentVariable(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : std::string();
}

// TempFile implementation
TempFile::TempFile(const std::string& content) {
    filepath_ = TestHelpers::CreateTempFile(content);
}

TempFile::~TempFile() {
    TestHelpers::CleanupTempPath(filepath_);
}

void TempFile::write(const std::string& content) {
    TestHelpers::WriteFile(filepath_, content);
}

std::string TempFile::read() const {
    return TestHelpers::ReadFile(filepath_);
}

// TempDir implementation
TempDir::TempDir() {
    dirpath_ = TestHelpers::CreateTempDir();
}

TempDir::~TempDir() {
    TestHelpers::CleanupTempPath(dirpath_);
}

std::string TempDir::create_file(const std::string& filename, const std::string& content) {
    std::string filepath = dirpath_ + "/" + filename;
    TestHelpers::WriteFile(filepath, content);
    return filepath;
}

// EnvVar implementation
EnvVar::EnvVar(const std::string& name, const std::string& value)
    : name_(name) {
    original_value_ = TestHelpers::GetEnvironmentVariable(name_);
    had_original_ = !original_value_.empty();
    TestHelpers::SetEnvironmentVariable(name_, value);
}

EnvVar::~EnvVar() {
    if (had_original_) {
        TestHelpers::SetEnvironmentVariable(name_, original_value_);
    } else {
        TestHelpers::UnsetEnvironmentVariable(name_);
    }
}

} // namespace test
} // namespace llm