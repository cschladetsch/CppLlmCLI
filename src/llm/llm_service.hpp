#pragma once

#include <coroutine>
#include <functional>
#include <future>
#include <memory>
#include <string>

#include "models/conversation.hpp"
#include "models/message.hpp"

namespace llm {

struct CompletionResponse {
  std::string content;
  bool success;
  std::string error;
  size_t tokens_used = 0;
  std::string model;
};

struct ModelInfo {
  std::string id;
  std::string name;
  size_t context_length;
  bool supports_streaming;
};

using StreamCallback =
    std::function<void(const std::string &chunk, bool is_done)>;

class LLMService {
public:
  virtual ~LLMService() = default;

  virtual std::future<CompletionResponse>
  complete_async(const Conversation &conversation) = 0;

  virtual CompletionResponse complete(const Conversation &conversation) = 0;

  virtual CompletionResponse complete(const std::string &prompt) = 0;

  virtual void stream_complete(const Conversation &conversation,
                               StreamCallback callback) = 0;

  virtual void stream_complete(const std::string &prompt,
                               StreamCallback callback) = 0;

  virtual std::vector<ModelInfo> get_available_models() = 0;

  virtual void set_model(const std::string &model_id) = 0;
  virtual std::string get_current_model() const = 0;

  virtual void set_temperature(float temperature) = 0;
  virtual void set_max_tokens(size_t max_tokens) = 0;
  virtual void set_system_prompt(const std::string &prompt) = 0;

  virtual bool is_available() = 0;

protected:
  std::string current_model_;
  float temperature_ = 0.7f;
  size_t max_tokens_ = 2048;
  std::string system_prompt_ = "You are a helpful AI assistant.";
};

class ServiceFactory {
public:
  enum class Provider { Groq, Together, Ollama };

  static std::unique_ptr<LLMService> create(Provider provider,
                                            const std::string &api_key = "",
                                            const std::string &base_url = "");

  static Provider string_to_provider(const std::string &provider_str);
  static std::string provider_to_string(Provider provider);
};

template <typename T> struct Task {
  struct promise_type {
    T value_;
    std::exception_ptr exception_;

    Task get_return_object() {
      return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
    }

    std::suspend_never initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }

    void return_value(T value) { value_ = std::move(value); }

    void unhandled_exception() { exception_ = std::current_exception(); }
  };

  std::coroutine_handle<promise_type> h_;

  Task(std::coroutine_handle<promise_type> h) : h_(h) {}
  ~Task() {
    if (h_)
      h_.destroy();
  }

  Task(const Task &) = delete;
  Task &operator=(const Task &) = delete;

  Task(Task &&other) noexcept : h_(std::exchange(other.h_, {})) {}
  Task &operator=(Task &&other) noexcept {
    if (this != &other) {
      if (h_)
        h_.destroy();
      h_ = std::exchange(other.h_, {});
    }
    return *this;
  }

  T get() {
    if (!h_.done()) {
      throw std::runtime_error("Coroutine not finished");
    }
    if (h_.promise().exception_) {
      std::rethrow_exception(h_.promise().exception_);
    }
    return std::move(h_.promise().value_);
  }

  bool done() const { return h_.done(); }
};

} // namespace llm