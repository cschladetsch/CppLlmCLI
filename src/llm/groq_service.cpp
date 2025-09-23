#include "utils/logger.hpp"
#include "llm/groq_service.hpp"

#include <iostream>

namespace llm {

const std::vector<ModelInfo> GroqService::AVAILABLE_MODELS = {
    {"llama-3.3-70b-versatile", "Llama 3.3 70B", 131072, true},
    {"llama-3.1-70b-versatile", "Llama 3.1 70B (Deprecated)", 131072, false},
    {"llama-3.1-8b-instant", "Llama 3.1 8B", 131072, true},
    {"mixtral-8x7b-32768", "Mixtral 8x7B", 32768, true},
    {"gemma2-9b-it", "Gemma 2 9B", 8192, true}};

GroqService::GroqService(const std::string &api_key,
                         const std::string &base_url)
    : api_key_(api_key) {
  http_client_ = std::make_unique<HttpClient>(base_url);
  http_client_->set_bearer_token(api_key_);
  current_model_ = "llama-3.3-70b-versatile";
}

std::future<CompletionResponse>
GroqService::complete_async(const Conversation &conversation) {
  return std::async(std::launch::async,
                    [this, conversation]() { return complete(conversation); });
}

CompletionResponse GroqService::complete(const Conversation &conversation) {
  auto request_data = prepare_request(conversation);

  LOG_DEBUG("Making request to /openai/v1/chat/completions");
  LOG_DEBUG("Request data: {}...", request_data.dump().substr(0, 200));

  auto response = http_client_->post("/openai/v1/chat/completions", request_data);

  LOG_DEBUG("Response status: {}", response.status_code);
  LOG_DEBUG("Response success: {}", response.success);
  LOG_DEBUG("Response body length: {}", response.body.length());
  if (!response.body.empty()) {
    LOG_DEBUG("Response body preview: {}", response.body.substr(0, 200));
  }
  if (!response.error.empty()) {
    LOG_ERROR("Response error: {}", response.error);
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

  http_client_->post_stream("/openai/v1/chat/completions", request_data,
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
  // Accept any model name from config - let the API validate it
  current_model_ = model_id;
  std::cout << "[INFO] Switched to model: " << model_id << "\n";
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
  // For Windows, just return true - we'll handle errors when actually making requests
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