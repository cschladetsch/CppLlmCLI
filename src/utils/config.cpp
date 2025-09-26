#include "utils/config.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include "utils/logger.hpp"

namespace llm {

Config::Config(const std::string &config_file) {
  setup_default_configs();
  load_from_file(config_file);
  set_from_environment();
}

bool Config::load_from_file(const std::string &filename) {
  try {
    std::ifstream file(expand_path(filename));
    if (!file.is_open()) {
      spdlog::debug("Config file not found: {}", filename);
      return false;
    }

    nlohmann::json j;
    file >> j;
    from_json(j);

    spdlog::info("Config loaded from: {}", filename);
    return true;
  } catch (const std::exception &e) {
    spdlog::error("Error loading config: {}", e.what());
    return false;
  }
}

bool Config::save_to_file(const std::string &filename) const {
  try {
    std::filesystem::path file_path = expand_path(filename);
    std::filesystem::create_directories(file_path.parent_path());

    std::ofstream file(file_path);
    if (!file.is_open()) {
      spdlog::error("Failed to create config file: {}", filename);
      return false;
    }

    file << to_json().dump(2);
    spdlog::info("Config saved to: {}", filename);
    return true;
  } catch (const std::exception &e) {
    spdlog::error("Error saving config: {}", e.what());
    return false;
  }
}

std::string Config::get_api_key() const {
  if (!api_key_.empty()) {
    return api_key_;
  }

  std::string env_key;
  if (provider_ == "groq") {
    env_key = get_env_var("GROQ_API_KEY");
  } else if (provider_ == "together") {
    env_key = get_env_var("TOGETHER_API_KEY");
  }

  return env_key;
}

ProviderConfig Config::get_provider_config(const std::string &provider) const {
  auto it = provider_configs_.find(provider);
  if (it != provider_configs_.end()) {
    return it->second;
  }

  ProviderConfig config;
  if (provider == "groq") {
    config.model = "llama-3.3-70b-versatile";
    config.api_url = "https://api.groq.com/openai/v1";
  } else if (provider == "together") {
    config.model = "meta-llama/Llama-2-70b-chat-hf";
    config.api_url = "https://api.together.xyz/v1";
  } else if (provider == "ollama") {
    config.model = "llama3.1";
    config.api_url = "http://localhost:11434";
  }

  return config;
}

void Config::set_provider_config(const std::string &provider,
                                 const ProviderConfig &config) {
  provider_configs_[provider] = config;
}

void Config::set_from_environment() {
  std::string env_provider = get_env_var("LLM_PROVIDER");
  if (!env_provider.empty()) {
    provider_ = env_provider;
  }

  std::string env_api_key = get_api_key();
  if (!env_api_key.empty()) {
    api_key_ = env_api_key;
  }
}

void Config::merge_command_line_args(
    const std::map<std::string, std::string> &args) {
  auto it = args.find("provider");
  if (it != args.end()) {
    provider_ = it->second;
  }

  it = args.find("model");
  if (it != args.end()) {
    auto config = get_provider_config(provider_);
    config.model = it->second;
    set_provider_config(provider_, config);
  }

  it = args.find("api-key");
  if (it != args.end()) {
    api_key_ = it->second;
  }

  it = args.find("temperature");
  if (it != args.end()) {
    try {
      float temp = std::stof(it->second);
      auto config = get_provider_config(provider_);
      config.temperature = temp;
      set_provider_config(provider_, config);
    } catch (const std::exception &e) {
      std::cout << "[WARN] Invalid temperature value: " << it->second << "\n";
    }
  }
}

nlohmann::json Config::to_json() const {
  nlohmann::json j;
  j["provider"] = provider_;

  if (!api_key_.empty()) {
    j["api_key"] = api_key_;
  }

  for (const auto &[provider, config] : provider_configs_) {
    nlohmann::json provider_json;
    provider_json["model"] = config.model;
    provider_json["temperature"] = config.temperature;
    provider_json["max_tokens"] = config.max_tokens;
    provider_json["api_url"] = config.api_url;

    if (!config.extra_params.empty()) {
      provider_json["extra_params"] = config.extra_params;
    }

    j[provider] = provider_json;
  }

  nlohmann::json repl_json;
  repl_json["history_file"] = repl_config_.history_file;
  repl_json["max_history"] = repl_config_.max_history;
  repl_json["system_prompt"] = repl_config_.system_prompt;
  repl_json["streaming"] = repl_config_.streaming;
  repl_json["markdown_rendering"] = repl_config_.markdown_rendering;
  repl_json["prompt_prefix"] = repl_config_.prompt_prefix;
  repl_json["ai_prefix"] = repl_config_.ai_prefix;

  j["repl"] = repl_json;

  return j;
}

void Config::from_json(const nlohmann::json &j) {
  if (j.contains("provider")) {
    provider_ = j["provider"];
  }

  if (j.contains("api_key")) {
    api_key_ = j["api_key"];
  }

  for (const auto &provider : {"groq", "together", "ollama"}) {
    if (j.contains(provider)) {
      ProviderConfig config;
      auto provider_json = j[provider];

      if (provider_json.contains("model")) {
        config.model = provider_json["model"];
      }
      if (provider_json.contains("temperature")) {
        config.temperature = provider_json["temperature"];
      }
      if (provider_json.contains("max_tokens")) {
        config.max_tokens = provider_json["max_tokens"];
      }
      if (provider_json.contains("api_url")) {
        config.api_url = provider_json["api_url"];
      }
      if (provider_json.contains("extra_params")) {
        config.extra_params = provider_json["extra_params"];
      }

      provider_configs_[provider] = config;
    }
  }

  if (j.contains("repl")) {
    auto repl_json = j["repl"];
    if (repl_json.contains("history_file")) {
      repl_config_.history_file = repl_json["history_file"];
    }
    if (repl_json.contains("max_history")) {
      repl_config_.max_history = repl_json["max_history"];
    }
    if (repl_json.contains("system_prompt")) {
      repl_config_.system_prompt = repl_json["system_prompt"];
    }
    if (repl_json.contains("streaming")) {
      repl_config_.streaming = repl_json["streaming"];
    }
    if (repl_json.contains("markdown_rendering")) {
      repl_config_.markdown_rendering = repl_json["markdown_rendering"];
    }
    if (repl_json.contains("prompt_prefix")) {
      repl_config_.prompt_prefix = repl_json["prompt_prefix"];
    }
    if (repl_json.contains("ai_prefix")) {
      repl_config_.ai_prefix = repl_json["ai_prefix"];
    }
  }
}

std::string Config::expand_path(const std::string &path) const {
  if (path.empty())
    return path;

  std::string expanded = path;
  if (expanded[0] == '~') {
    const char *home = std::getenv("HOME");
    if (!home) {
      home = std::getenv("USERPROFILE");
    }
    if (home) {
      expanded = std::string(home) + expanded.substr(1);
    }
  }

  return std::filesystem::absolute(expanded).string();
}

void Config::setup_default_configs() {
  ProviderConfig groq_config;
  groq_config.model = "llama-3.1-70b-versatile";
  groq_config.api_url = "https://api.groq.com/openai/v1";
  provider_configs_["groq"] = groq_config;

  ProviderConfig together_config;
  together_config.model = "meta-llama/Llama-2-70b-chat-hf";
  together_config.api_url = "https://api.together.xyz/v1";
  provider_configs_["together"] = together_config;

  ProviderConfig ollama_config;
  ollama_config.model = "llama3.1";
  ollama_config.api_url = "http://localhost:11434";
  provider_configs_["ollama"] = ollama_config;
}

std::string Config::get_env_var(const std::string &name) const {
  const char *value = std::getenv(name.c_str());
  return value ? std::string(value) : std::string();
}

} // namespace llm