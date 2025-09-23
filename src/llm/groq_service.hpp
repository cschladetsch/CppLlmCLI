#pragma once

#include <memory>

#include "http/http_client.hpp"
#include "llm/llm_service.hpp"

namespace llm {

class GroqService : public LLMService {
public:
  explicit GroqService(
      const std::string &api_key,
      const std::string &base_url = "https://api.groq.com/openai/v1");

  std::future<CompletionResponse>
  complete_async(const Conversation &conversation) override;

  CompletionResponse complete(const Conversation &conversation) override;

  CompletionResponse complete(const std::string &prompt) override;

  void stream_complete(const Conversation &conversation,
                       StreamCallback callback) override;

  void stream_complete(const std::string &prompt,
                       StreamCallback callback) override;

  std::vector<ModelInfo> get_available_models() override;

  void set_model(const std::string &model_id) override;
  std::string get_current_model() const override;

  void set_temperature(float temperature) override;
  void set_max_tokens(size_t max_tokens) override;
  void set_system_prompt(const std::string &prompt) override;

  bool is_available() override;

private:
  std::unique_ptr<HttpClient> http_client_;
  std::string api_key_;

  nlohmann::json prepare_request(const Conversation &conversation,
                                 bool stream = false);
  CompletionResponse parse_response(const HttpClient::Response &response);

  static const std::vector<ModelInfo> AVAILABLE_MODELS;
};

} // namespace llm