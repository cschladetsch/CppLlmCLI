#pragma once

#include "models/conversation.hpp"
#include "models/message.hpp"
#include "utils/config.hpp"
#include <string>
#include <memory>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <nlohmann/json.hpp>
#include <filesystem>

namespace llm {
namespace test {

class TestHelpers {
public:
    // Create test configuration
    static std::unique_ptr<Config> CreateTestConfig();

    // Create test conversation with sample messages
    static Conversation CreateTestConversation();

    // Create temporary test file
    static std::string CreateTempFile(const std::string& content = "");

    // Create temporary test directory
    static std::string CreateTempDir();

    // Clean up temporary files and directories
    static void CleanupTempPath(const std::string& path);

    // JSON comparison helpers
    static bool JsonEquals(const nlohmann::json& a, const nlohmann::json& b);

    // String utilities
    static std::string TrimWhitespace(const std::string& str);
    static std::vector<std::string> SplitString(const std::string& str, const std::string& delimiter);

    // File utilities
    static std::string ReadFile(const std::string& filepath);
    static void WriteFile(const std::string& filepath, const std::string& content);
    static bool FileExists(const std::string& filepath);

    // Test data generators
    static std::vector<Message> GenerateTestMessages(size_t count);
    static nlohmann::json GenerateTestApiResponse(const std::string& content);
    static nlohmann::json GenerateTestErrorResponse(const std::string& error);

    // Mock HTTP responses
    static std::string CreateMockGroqResponse(const std::string& content, size_t tokens = 100);
    static std::string CreateMockStreamingChunk(const std::string& content);
    static std::string CreateMockModelsResponse();

    // Environment helpers
    static void SetEnvironmentVariable(const std::string& name, const std::string& value);
    static void UnsetEnvironmentVariable(const std::string& name);
    static std::string GetEnvironmentVariable(const std::string& name);

private:
    static std::vector<std::string> temp_paths_;
};

// RAII helper for temporary files
class TempFile {
public:
    explicit TempFile(const std::string& content = "");
    ~TempFile();

    const std::string& path() const { return filepath_; }
    void write(const std::string& content);
    std::string read() const;

private:
    std::string filepath_;
};

// RAII helper for temporary directories
class TempDir {
public:
    TempDir();
    ~TempDir();

    const std::string& path() const { return dirpath_; }
    std::string create_file(const std::string& filename, const std::string& content = "");

private:
    std::string dirpath_;
};

// RAII helper for environment variables
class EnvVar {
public:
    EnvVar(const std::string& name, const std::string& value);
    ~EnvVar();

private:
    std::string name_;
    std::string original_value_;
    bool had_original_;
};

// Custom matchers for testing
MATCHER_P(HasMessageCount, count, "") {
    return arg.size() == count;
}

MATCHER_P(ContainsMessage, content, "") {
    for (const auto& msg : arg.messages()) {
        if (msg.content.find(content) != std::string::npos) {
            return true;
        }
    }
    return false;
}

MATCHER_P(HasRole, role, "") {
    return arg.role == role;
}

MATCHER_P(JsonContains, key, "") {
    return arg.contains(key);
}

MATCHER_P2(JsonHasValue, key, value, "") {
    return arg.contains(key) && arg[key] == value;
}

} // namespace test
} // namespace llm