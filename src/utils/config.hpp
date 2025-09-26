#pragma once

#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace llm {

struct ProviderConfig {
  std::string model;
  float temperature = 0.7f;
  size_t max_tokens = 2048;
  std::string api_url;
  std::map<std::string, std::string> extra_params;
};

struct ReplConfig {
  std::string history_file = "~/.llm_repl_history";
  size_t max_history = 100;
  std::string system_prompt = "You are a helpful AI assistant.";
  bool streaming = true;
  bool markdown_rendering = true;
  std::string prompt_prefix = "> ";
  std::string ai_prefix = "AI: ";
};

class Config {
public:
  Config() = default;
  explicit Config(const std::string &config_file);

  bool load_from_file(const std::string &filename);
  bool save_to_file(const std::string &filename) const;

  void set_provider(const std::string &provider) { provider_ = provider; }
  std::string get_provider() const { return provider_; }

  void set_api_key(const std::string &key) { api_key_ = key; }
  std::string get_api_key() const;

  ProviderConfig get_provider_config(const std::string &provider) const;
  void set_provider_config(const std::string &provider,
                           const ProviderConfig &config);

  const ReplConfig &get_repl_config() const { return repl_config_; }
  void set_repl_config(const ReplConfig &config) { repl_config_ = config; }

  void set_from_environment();
  void merge_command_line_args(const std::map<std::string, std::string> &args);

  nlohmann::json to_json() const;
  void from_json(const nlohmann::json &j);

  std::string expand_path(const std::string &path) const;

private:
  std::string provider_ = "groq";
  std::string api_key_;
  std::map<std::string, ProviderConfig> provider_configs_;
  ReplConfig repl_config_;

  void setup_default_configs();
  std::string get_env_var(const std::string &name) const;
};

} // namespace llm