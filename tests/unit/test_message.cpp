#include <gtest/gtest.h>
#include "models/message.hpp"
#include "utils/test_helpers.hpp"

using namespace llm;
using namespace llm::test;

class MessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        system_message_ = Message(MessageRole::System, "You are a helpful assistant.");
        user_message_ = Message(MessageRole::User, "Hello, how are you?");
        assistant_message_ = Message(MessageRole::Assistant, "I'm doing well, thank you!");
    }

    Message system_message_{MessageRole::System, ""};
    Message user_message_{MessageRole::User, ""};
    Message assistant_message_{MessageRole::Assistant, ""};
};

TEST_F(MessageTest, MessageConstruction) {
    EXPECT_EQ(system_message_.role, MessageRole::System);
    EXPECT_EQ(system_message_.content, "You are a helpful assistant.");

    EXPECT_EQ(user_message_.role, MessageRole::User);
    EXPECT_EQ(user_message_.content, "Hello, how are you?");

    EXPECT_EQ(assistant_message_.role, MessageRole::Assistant);
    EXPECT_EQ(assistant_message_.content, "I'm doing well, thank you!");
}

TEST_F(MessageTest, ToJsonConversion) {
    auto system_json = system_message_.to_json();
    EXPECT_EQ(system_json["role"], "system");
    EXPECT_EQ(system_json["content"], "You are a helpful assistant.");

    auto user_json = user_message_.to_json();
    EXPECT_EQ(user_json["role"], "user");
    EXPECT_EQ(user_json["content"], "Hello, how are you?");

    auto assistant_json = assistant_message_.to_json();
    EXPECT_EQ(assistant_json["role"], "assistant");
    EXPECT_EQ(assistant_json["content"], "I'm doing well, thank you!");
}

TEST_F(MessageTest, FromJsonConversion) {
    nlohmann::json system_json = {
        {"role", "system"},
        {"content", "Test system prompt"}
    };

    nlohmann::json user_json = {
        {"role", "user"},
        {"content", "Test user message"}
    };

    nlohmann::json assistant_json = {
        {"role", "assistant"},
        {"content", "Test assistant response"}
    };

    auto system_msg = Message::from_json(system_json);
    EXPECT_EQ(system_msg.role, MessageRole::System);
    EXPECT_EQ(system_msg.content, "Test system prompt");

    auto user_msg = Message::from_json(user_json);
    EXPECT_EQ(user_msg.role, MessageRole::User);
    EXPECT_EQ(user_msg.content, "Test user message");

    auto assistant_msg = Message::from_json(assistant_json);
    EXPECT_EQ(assistant_msg.role, MessageRole::Assistant);
    EXPECT_EQ(assistant_msg.content, "Test assistant response");
}

TEST_F(MessageTest, RoundTripJsonConversion) {
    // Test that converting to JSON and back preserves the message
    auto original_json = user_message_.to_json();
    auto reconstructed_message = Message::from_json(original_json);

    EXPECT_EQ(reconstructed_message.role, user_message_.role);
    EXPECT_EQ(reconstructed_message.content, user_message_.content);
}

TEST_F(MessageTest, UnknownRoleDefaultsToUser) {
    nlohmann::json unknown_role_json = {
        {"role", "unknown_role"},
        {"content", "Test content"}
    };

    auto message = Message::from_json(unknown_role_json);
    EXPECT_EQ(message.role, MessageRole::User);
    EXPECT_EQ(message.content, "Test content");
}

TEST_F(MessageTest, EmptyContent) {
    Message empty_message(MessageRole::User, "");

    EXPECT_EQ(empty_message.role, MessageRole::User);
    EXPECT_EQ(empty_message.content, "");

    auto json = empty_message.to_json();
    EXPECT_EQ(json["role"], "user");
    EXPECT_EQ(json["content"], "");
}

TEST_F(MessageTest, LongContent) {
    std::string long_content(10000, 'a'); // 10,000 character string
    Message long_message(MessageRole::Assistant, long_content);

    EXPECT_EQ(long_message.content.length(), 10000);

    auto json = long_message.to_json();
    EXPECT_EQ(json["content"], long_content);

    auto reconstructed = Message::from_json(json);
    EXPECT_EQ(reconstructed.content, long_content);
}

TEST_F(MessageTest, SpecialCharacters) {
    std::string special_content = "Special chars: \n\t\r\"\\\'{}[]";
    Message special_message(MessageRole::User, special_content);

    auto json = special_message.to_json();
    auto reconstructed = Message::from_json(json);

    EXPECT_EQ(reconstructed.content, special_content);
}

TEST_F(MessageTest, UnicodeContent) {
    std::string unicode_content = "Unicode: 🎉 🚀 🤖 こんにちは العالم";
    Message unicode_message(MessageRole::Assistant, unicode_content);

    auto json = unicode_message.to_json();
    auto reconstructed = Message::from_json(json);

    EXPECT_EQ(reconstructed.content, unicode_content);
}