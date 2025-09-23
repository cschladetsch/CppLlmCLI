#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "models/message.hpp"

namespace llm {

class Conversation {
public:
  Conversation() = default;

  void add_message(const Message &message) { messages_.push_back(message); }

  void add_system(const std::string &content) {
    messages_.emplace_back(MessageRole::System, content);
  }

  void add_user(const std::string &content) {
    messages_.emplace_back(MessageRole::User, content);
  }

  void add_assistant(const std::string &content) {
    messages_.emplace_back(MessageRole::Assistant, content);
  }

  void clear() { messages_.clear(); }

  void set_system_prompt(const std::string &prompt) {
    if (!messages_.empty() && messages_[0].role == MessageRole::System) {
      messages_[0].content = prompt;
    } else {
      messages_.insert(messages_.begin(), Message(MessageRole::System, prompt));
    }
  }

  const std::vector<Message> &messages() const { return messages_; }

  nlohmann::json to_json() const {
    nlohmann::json j = nlohmann::json::array();
    for (const auto &msg : messages_) {
      j.push_back(msg.to_json());
    }
    return j;
  }

  void from_json(const nlohmann::json &j) {
    messages_.clear();
    for (const auto &msg_json : j) {
      messages_.push_back(Message::from_json(msg_json));
    }
  }

  size_t size() const { return messages_.size(); }

  bool empty() const { return messages_.empty(); }

  size_t estimate_tokens() const {
    size_t tokens = 0;
    for (const auto &msg : messages_) {
      tokens += msg.content.length() / 4;
    }
    return tokens;
  }

  void truncate_to_token_limit(size_t max_tokens, size_t keep_recent = 10) {
    if (estimate_tokens() <= max_tokens) {
      return;
    }

    std::vector<Message> new_messages;

    if (!messages_.empty() && messages_[0].role == MessageRole::System) {
      new_messages.push_back(messages_[0]);
    }

    size_t start_idx =
        messages_.size() > keep_recent ? messages_.size() - keep_recent : 0;

    for (size_t i = start_idx; i < messages_.size(); ++i) {
      new_messages.push_back(messages_[i]);
    }

    messages_ = std::move(new_messages);
  }

  std::string to_string() const {
    std::string result;
    for (const auto &msg : messages_) {
      std::string role_str;
      switch (msg.role) {
      case MessageRole::System:
        role_str = "[System]";
        break;
      case MessageRole::User:
        role_str = "[User]";
        break;
      case MessageRole::Assistant:
        role_str = "[Assistant]";
        break;
      }
      result += role_str + " " + msg.content + "\n\n";
    }
    return result;
  }

  void save_to_file(const std::string &filename) const;
  void load_from_file(const std::string &filename);

private:
  std::vector<Message> messages_;
};

} // namespace llm