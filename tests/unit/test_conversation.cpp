#include <gtest/gtest.h>
#include "models/conversation.hpp"
#include "utils/test_helpers.hpp"

using namespace llm;
using namespace llm::test;

class ConversationTest : public ::testing::Test {
protected:
    void SetUp() override {
        conversation_ = std::make_unique<Conversation>();
    }

    std::unique_ptr<Conversation> conversation_;
};

TEST_F(ConversationTest, EmptyConversation) {
    EXPECT_TRUE(conversation_->empty());
    EXPECT_EQ(conversation_->size(), 0);
    EXPECT_EQ(conversation_->estimate_tokens(), 0);
}

TEST_F(ConversationTest, AddMessages) {
    conversation_->add_system("You are a helpful assistant.");
    conversation_->add_user("Hello!");
    conversation_->add_assistant("Hi there! How can I help you?");

    EXPECT_FALSE(conversation_->empty());
    EXPECT_EQ(conversation_->size(), 3);

    const auto& messages = conversation_->messages();
    EXPECT_EQ(messages[0].role, MessageRole::System);
    EXPECT_EQ(messages[0].content, "You are a helpful assistant.");

    EXPECT_EQ(messages[1].role, MessageRole::User);
    EXPECT_EQ(messages[1].content, "Hello!");

    EXPECT_EQ(messages[2].role, MessageRole::Assistant);
    EXPECT_EQ(messages[2].content, "Hi there! How can I help you?");
}

TEST_F(ConversationTest, AddMessageObject) {
    Message user_msg(MessageRole::User, "Test message");
    conversation_->add_message(user_msg);

    EXPECT_EQ(conversation_->size(), 1);
    EXPECT_EQ(conversation_->messages()[0].role, MessageRole::User);
    EXPECT_EQ(conversation_->messages()[0].content, "Test message");
}

TEST_F(ConversationTest, Clear) {
    conversation_->add_user("Hello");
    conversation_->add_assistant("Hi");

    EXPECT_EQ(conversation_->size(), 2);

    conversation_->clear();

    EXPECT_TRUE(conversation_->empty());
    EXPECT_EQ(conversation_->size(), 0);
}

TEST_F(ConversationTest, SetSystemPrompt) {
    // Setting system prompt on empty conversation
    conversation_->set_system_prompt("Initial system prompt");

    EXPECT_EQ(conversation_->size(), 1);
    EXPECT_EQ(conversation_->messages()[0].role, MessageRole::System);
    EXPECT_EQ(conversation_->messages()[0].content, "Initial system prompt");

    // Adding more messages
    conversation_->add_user("Hello");
    conversation_->add_assistant("Hi");

    EXPECT_EQ(conversation_->size(), 3);

    // Updating system prompt should modify existing system message
    conversation_->set_system_prompt("Updated system prompt");

    EXPECT_EQ(conversation_->size(), 3);
    EXPECT_EQ(conversation_->messages()[0].role, MessageRole::System);
    EXPECT_EQ(conversation_->messages()[0].content, "Updated system prompt");
}

TEST_F(ConversationTest, SetSystemPromptWithoutExisting) {
    conversation_->add_user("Hello");
    conversation_->set_system_prompt("New system prompt");

    EXPECT_EQ(conversation_->size(), 2);
    EXPECT_EQ(conversation_->messages()[0].role, MessageRole::System);
    EXPECT_EQ(conversation_->messages()[0].content, "New system prompt");
    EXPECT_EQ(conversation_->messages()[1].role, MessageRole::User);
    EXPECT_EQ(conversation_->messages()[1].content, "Hello");
}

TEST_F(ConversationTest, ToJson) {
    conversation_->add_system("System prompt");
    conversation_->add_user("User message");
    conversation_->add_assistant("Assistant response");

    auto json = conversation_->to_json();

    EXPECT_TRUE(json.is_array());
    EXPECT_EQ(json.size(), 3);

    EXPECT_EQ(json[0]["role"], "system");
    EXPECT_EQ(json[0]["content"], "System prompt");

    EXPECT_EQ(json[1]["role"], "user");
    EXPECT_EQ(json[1]["content"], "User message");

    EXPECT_EQ(json[2]["role"], "assistant");
    EXPECT_EQ(json[2]["content"], "Assistant response");
}

TEST_F(ConversationTest, FromJson) {
    nlohmann::json json_data = nlohmann::json::array({
        {{"role", "system"}, {"content", "System prompt"}},
        {{"role", "user"}, {"content", "User message"}},
        {{"role", "assistant"}, {"content", "Assistant response"}}
    });

    conversation_->from_json(json_data);

    EXPECT_EQ(conversation_->size(), 3);

    const auto& messages = conversation_->messages();
    EXPECT_EQ(messages[0].role, MessageRole::System);
    EXPECT_EQ(messages[0].content, "System prompt");

    EXPECT_EQ(messages[1].role, MessageRole::User);
    EXPECT_EQ(messages[1].content, "User message");

    EXPECT_EQ(messages[2].role, MessageRole::Assistant);
    EXPECT_EQ(messages[2].content, "Assistant response");
}

TEST_F(ConversationTest, RoundTripJsonConversion) {
    conversation_->add_system("System");
    conversation_->add_user("User");
    conversation_->add_assistant("Assistant");

    auto json = conversation_->to_json();

    Conversation new_conversation;
    new_conversation.from_json(json);

    EXPECT_EQ(new_conversation.size(), conversation_->size());

    const auto& original_messages = conversation_->messages();
    const auto& new_messages = new_conversation.messages();

    for (size_t i = 0; i < original_messages.size(); ++i) {
        EXPECT_EQ(new_messages[i].role, original_messages[i].role);
        EXPECT_EQ(new_messages[i].content, original_messages[i].content);
    }
}

TEST_F(ConversationTest, EstimateTokens) {
    EXPECT_EQ(conversation_->estimate_tokens(), 0);

    conversation_->add_user("Hello"); // ~1.25 tokens
    EXPECT_GT(conversation_->estimate_tokens(), 0);

    size_t tokens_after_first = conversation_->estimate_tokens();

    conversation_->add_assistant("Hello! How can I help you today?"); // ~8 tokens
    EXPECT_GT(conversation_->estimate_tokens(), tokens_after_first);
}

TEST_F(ConversationTest, TruncateToTokenLimit) {
    // Add many messages
    conversation_->add_system("System prompt");
    for (int i = 0; i < 20; ++i) {
        conversation_->add_user("User message " + std::to_string(i));
        conversation_->add_assistant("Assistant response " + std::to_string(i));
    }

    size_t original_size = conversation_->size();
    EXPECT_GT(original_size, 10);

    // Truncate to a small token limit
    conversation_->truncate_to_token_limit(50, 5);

    EXPECT_LT(conversation_->size(), original_size);

    // Should keep system message
    const auto& messages = conversation_->messages();
    EXPECT_EQ(messages[0].role, MessageRole::System);

    // Should keep recent messages
    EXPECT_LE(conversation_->size(), 6); // system + 5 recent
}

TEST_F(ConversationTest, ToString) {
    conversation_->add_system("System prompt");
    conversation_->add_user("Hello");
    conversation_->add_assistant("Hi there!");

    std::string str = conversation_->to_string();

    EXPECT_THAT(str, HasSubstr("[System]"));
    EXPECT_THAT(str, HasSubstr("System prompt"));
    EXPECT_THAT(str, HasSubstr("[User]"));
    EXPECT_THAT(str, HasSubstr("Hello"));
    EXPECT_THAT(str, HasSubstr("[Assistant]"));
    EXPECT_THAT(str, HasSubstr("Hi there!"));
}

TEST_F(ConversationTest, SaveAndLoadFile) {
    TempFile temp_file;

    conversation_->add_system("System prompt");
    conversation_->add_user("Hello");
    conversation_->add_assistant("Hi there!");

    // Save conversation
    conversation_->save_to_file(temp_file.path());

    // Create new conversation and load
    Conversation loaded_conversation;
    loaded_conversation.load_from_file(temp_file.path());

    EXPECT_EQ(loaded_conversation.size(), conversation_->size());

    const auto& original_messages = conversation_->messages();
    const auto& loaded_messages = loaded_conversation.messages();

    for (size_t i = 0; i < original_messages.size(); ++i) {
        EXPECT_EQ(loaded_messages[i].role, original_messages[i].role);
        EXPECT_EQ(loaded_messages[i].content, original_messages[i].content);
    }
}

TEST_F(ConversationTest, LoadNonexistentFile) {
    // Loading a nonexistent file should not crash
    EXPECT_NO_THROW(conversation_->load_from_file("/nonexistent/file.json"));
    EXPECT_TRUE(conversation_->empty());
}

TEST_F(ConversationTest, SaveToInvalidPath) {
    conversation_->add_user("Test");

    // Saving to invalid path should not crash
    EXPECT_NO_THROW(conversation_->save_to_file("/invalid/path/file.json"));
}