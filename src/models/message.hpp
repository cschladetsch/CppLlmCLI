#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace llm {

enum class MessageRole { System, User, Assistant };

struct Message {
  MessageRole role;
  std::string content;

  Message(MessageRole r, const std::string &c) : role(r), content(c) {}

  nlohmann::json to_json() const {
    nlohmann::json j;
    j["role"] = role_to_string(role);
    j["content"] = content;
    return j;
  }

  static Message from_json(const nlohmann::json &j) {
    return Message(string_to_role(j["role"]), j["content"]);
  }

private:
  static std::string role_to_string(MessageRole role) {
    switch (role) {
    case MessageRole::System:
      return "system";
    case MessageRole::User:
      return "user";
    case MessageRole::Assistant:
      return "assistant";
    default:
      return "user";
    }
  }

  static MessageRole string_to_role(const std::string &role_str) {
    if (role_str == "system")
      return MessageRole::System;
    if (role_str == "assistant")
      return MessageRole::Assistant;
    return MessageRole::User;
  }
};

} // namespace llm