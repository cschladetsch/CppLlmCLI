#include "llm/groq_service.hpp"

#include <iostream>
#include "utils/logger.hpp"

namespace llm {

const std::vector<ModelInfo> GroqService::AVAILABLE_MODELS = {
    {"llama-3.3-70b-versatile", "Llama 3.3 70B", 131072, true},
    {"llama-3.1-70b-versatile", "Llama 3.1 70B", 131072, true},
    {"llama-3.1-8b-instant", "Llama 3.1 8B", 131072, true},
    {"mixtral-8x7b-32768", "Mixtral 8x7B", 32768, true},
    {"gemma2-9b-it", "Gemma 2 9B", 8192, true}};

GroqService::GroqService(const std::string &api_key,
                         const std::string &base_url)
    : api_key_(api_key) {
  spdlog::debug("Initializing GroqService...");
  spdlog::debug("API URL: {}", base_url);
  spdlog::debug("API Key: {} (length: {})",
                Logger::safe_api_key(api_key), api_key.length());

  if (api_key.empty()) {
    spdlog::error("API Key is EMPTY!");
  }

  http_client_ = std::make_unique<HttpClient>(base_url);
  http_client_->set_bearer_token(api_key_);
  current_model_ = "llama-3.3-70b-versatile";

  spdlog::debug("Default model set to: {}", current_model_);
}

std::future<CompletionResponse>
GroqService::complete_async(const Conversation &conversation) {
  return std::async(std::launch::async,
                    [this, conversation]() { return complete(conversation); });
}

CompletionResponse GroqService::complete(const Conversation &conversation) {
  spdlog::debug("Preparing completion request...");
  auto request_data = prepare_request(conversation);

  spdlog::debug("Sending POST to /chat/completions...");
  auto response = http_client_->post("/chat/completions", request_data);

  spdlog::debug("Response received - Status: {}", response.status_code);
  if (!response.success) {
    spdlog::error("Request failed: {}", response.error);
  }

  return parse_response(response);
}

CompletionResponse GroqService::complete(const std::string &prompt) {
  Conversation conv;
  if (!system_prompt_.empty()) {
    conv.add_system(system_prompt_);
  }
  conv.add_user(prompt);
  return complete(conv);
}

void GroqService::stream_complete(const Conversation &conversation,
                                  StreamCallback callback) {
  auto request_data = prepare_request(conversation, true);

  http_client_->post_stream("/chat/completions", request_data,
                            [callback](const std::string &chunk, bool is_done) {
                              callback(chunk, is_done);
                            });
}

void GroqService::stream_complete(const std::string &prompt,
                                  StreamCallback callback) {
  Conversation conv;
  if (!system_prompt_.empty()) {
    conv.add_system(system_prompt_);
  }
  conv.add_user(prompt);
  stream_complete(conv, callback);
}

std::vector<ModelInfo> GroqService::get_available_models() {
  return AVAILABLE_MODELS;
}

void GroqService::set_model(const std::string &model_id) {
  current_model_ = model_id;
  spdlog::info("Switched to model: {}", model_id);
}

std::string GroqService::get_current_model() const { return current_model_; }

void GroqService::set_temperature(float temperature) {
  temperature_ = std::clamp(temperature, 0.0f, 2.0f);
}

void GroqService::set_max_tokens(size_t max_tokens) {
  max_tokens_ = std::min(max_tokens, static_cast<size_t>(8192));
}

void GroqService::set_system_prompt(const std::string &prompt) {
  system_prompt_ = prompt;
}

bool GroqService::is_available() {
  spdlog::debug("Checking Groq API availability...");

  // For Groq, we'll skip the health check and assume it's available
  // The /models endpoint doesn't exist for Groq's OpenAI-compatible API
  spdlog::info("Groq API assumed available (health check bypassed)");
  return true;
}

nlohmann::json GroqService::prepare_request(const Conversation &conversation,
                                            bool stream) {
  nlohmann::json request;
  request["model"] = current_model_;
  request["messages"] = conversation.to_json();
  request["temperature"] = temperature_;
  request["max_tokens"] = max_tokens_;
  request["stream"] = stream;

  return request;
}

CompletionResponse
GroqService::parse_response(const HttpClient::Response &response) {
  CompletionResponse result;

  if (!response.success) {
    result.success = false;
    result.error = response.error;
    return result;
  }

  try {
    auto json_response = nlohmann::json::parse(response.body);

    if (json_response.contains("choices") &&
        !json_response["choices"].empty() &&
        json_response["choices"][0].contains("message") &&
        json_response["choices"][0]["message"].contains("content")) {
      result.content = json_response["choices"][0]["message"]["content"];
      result.success = true;
      result.model = current_model_;

      if (json_response.contains("usage") &&
          json_response["usage"].contains("total_tokens")) {
        result.tokens_used = json_response["usage"]["total_tokens"];
      }
    } else {
      result.success = false;
      result.error = "Invalid response format";
    }
  } catch (const nlohmann::json::exception &e) {
    result.success = false;
    result.error = "JSON parsing error: " + std::string(e.what());
  }

  return result;
}

} // namespace llm