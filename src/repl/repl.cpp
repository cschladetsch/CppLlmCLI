#include "repl/repl.hpp"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <thread>

#include "llm/groq_service.hpp"
#include "utils/logger.hpp"

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

namespace llm {

REPL *REPL::instance_ = nullptr;

REPL::REPL(std::unique_ptr<Config> config) : config_(std::move(config)) {
  instance_ = this;
  setup_signal_handlers();

  spdlog::debug("REPL initialization starting...");
  spdlog::debug("Provider: {}", config_->get_provider());

  auto provider_config = config_->get_provider_config(config_->get_provider());
  conversation_.set_system_prompt(config_->get_repl_config().system_prompt);

  if (config_->get_provider() == "groq") {
    std::string api_key = config_->get_api_key();
    spdlog::debug("Creating GroqService with:");
    spdlog::debug("  API URL: {}", provider_config.api_url);
    spdlog::debug("  Model: {}", provider_config.model);
    spdlog::debug("  Temperature: {}", provider_config.temperature);
    spdlog::debug("  Max tokens: {}", provider_config.max_tokens);
    spdlog::debug("  API Key loaded: {}", api_key.empty() ? "NO (EMPTY!)" : "YES");

    llm_service_ = std::make_unique<GroqService>(api_key, provider_config.api_url);
    llm_service_->set_model(provider_config.model);
    llm_service_->set_temperature(provider_config.temperature);
    llm_service_->set_max_tokens(provider_config.max_tokens);
  }

  load_history();
}

REPL::~REPL() {
  cleanup();
  instance_ = nullptr;
}

void REPL::run() {
  if (!llm_service_) {
    spdlog::error("LLM service was not created!");
    std::cerr << colorize_text("Error: LLM service was not created!", "red") << std::endl;
    return;
  }

  spdlog::debug("Checking if LLM service is available...");
  if (!llm_service_->is_available()) {
    spdlog::error("Service availability check failed!");
    std::cerr << colorize_text("Error: LLM service is not available. Please "
                               "check your configuration and "
                               "API key.",
                               "red")
              << std::endl;
    return;
  }
  spdlog::debug("LLM service is available and ready");

  running_ = true;
  print_welcome();

  while (running_) {
    try {
      std::string input = read_input();

      if (input.empty()) {
        continue;
      }

      add_to_history(input);

      if (!process_command(input)) {
        break;
      }
    } catch (const std::exception &e) {
      std::cerr << colorize_text("Error: " + std::string(e.what()), "red")
                << std::endl;
    }
  }

  cleanup();
}

void REPL::stop() { running_ = false; }

void REPL::set_llm_service(std::unique_ptr<LLMService> service) {
  llm_service_ = std::move(service);
}

void REPL::print_welcome() {
  std::cout << colorize_text("LLM REPL v1.0.0", "cyan") << std::endl;
  std::cout << colorize_text("Provider: " + config_->get_provider(), "yellow")
            << std::endl;
  std::cout << colorize_text("Model: " + llm_service_->get_current_model(),
                             "yellow")
            << std::endl;
  std::cout << colorize_text("Type '/help' for commands or '/exit' to quit.",
                             "green")
            << std::endl;
  std::cout << std::endl;
}

void REPL::print_help() {
  std::cout << colorize_text("Available commands:", "cyan") << std::endl;
  std::cout << "  /help           - Show this help message" << std::endl;
  std::cout << "  /clear          - Clear conversation history" << std::endl;
  std::cout << "  /history        - Show conversation history" << std::endl;
  std::cout << "  /save [file]    - Save conversation to file" << std::endl;
  std::cout << "  /load [file]    - Load conversation from file" << std::endl;
  std::cout << "  /model [name]   - Switch to different model" << std::endl;
  std::cout << "  /system [prompt]- Set system prompt" << std::endl;
  std::cout << "  /exit           - Exit the REPL" << std::endl;
  std::cout << std::endl;
}

std::string REPL::read_input() {
  std::cout << colorize_text(config_->get_repl_config().prompt_prefix, "blue");
  std::cout.flush();

  std::string input;
  if (!std::getline(std::cin, input)) {
    // Check if we've reached EOF or if cin is in a bad state
    if (std::cin.eof()) {
      std::cout << std::endl << colorize_text("EOF detected. Exiting...", "yellow") << std::endl;
      running_ = false;
      return "";
    }

    // Clear error flags and try again
    std::cin.clear();
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    return "";
  }

  return input;
}

bool REPL::process_command(const std::string &input) {
  if (input.starts_with("/")) {
    return handle_slash_command(input);
  } else {
    process_user_input(input);
    return true;
  }
}

void REPL::process_user_input(const std::string &input) {
  if (processing_) {
    std::cout << colorize_text(
                     "Please wait for the current request to complete.",
                     "yellow")
              << std::endl;
    return;
  }

  processing_ = true;

  try {
    conversation_.add_user(input);

    if (config_->get_repl_config().streaming) {
      print_streaming_response(input);
    } else {
      auto response = llm_service_->complete(conversation_);
      print_response(response);

      if (response.success) {
        conversation_.add_assistant(response.content);
      }
    }
  } catch (const std::exception &e) {
    std::cerr << colorize_text("Error processing request: " +
                                   std::string(e.what()),
                               "red")
              << std::endl;
  }

  processing_ = false;
}

bool REPL::handle_slash_command(const std::string &command) {
  std::istringstream iss(command);
  std::string cmd;
  iss >> cmd;

  if (cmd == "/help") {
    handle_help_command();
  } else if (cmd == "/clear") {
    handle_clear_command();
  } else if (cmd == "/history") {
    handle_history_command();
  } else if (cmd == "/save") {
    std::string filename;
    iss >> filename;
    if (filename.empty()) {
      filename = "conversation.json";
    }
    handle_save_command(filename);
  } else if (cmd == "/load") {
    std::string filename;
    iss >> filename;
    if (filename.empty()) {
      std::cout << colorize_text("Usage: /load <filename>", "yellow")
                << std::endl;
    } else {
      handle_load_command(filename);
    }
  } else if (cmd == "/model") {
    std::string model_name;
    iss >> model_name;
    if (model_name.empty()) {
      auto models = llm_service_->get_available_models();
      std::cout << colorize_text("Available models:", "cyan") << std::endl;
      for (const auto &model : models) {
        std::cout << "  " << model.id << " - " << model.name << std::endl;
      }
    } else {
      handle_model_command(model_name);
    }
  } else if (cmd == "/system") {
    std::string prompt;
    std::getline(iss, prompt);
    if (!prompt.empty() && prompt[0] == ' ') {
      prompt = prompt.substr(1);
    }
    if (prompt.empty()) {
      std::cout << colorize_text("Usage: /system <prompt>", "yellow")
                << std::endl;
    } else {
      handle_system_command(prompt);
    }
  } else if (cmd == "/exit") {
    handle_exit_command();
    return false;
  } else {
    std::cout << colorize_text("Unknown command: " + cmd, "red") << std::endl;
    std::cout << colorize_text("Type '/help' for available commands.", "yellow")
              << std::endl;
  }

  return true;
}

void REPL::handle_help_command() { print_help(); }

void REPL::handle_clear_command() {
  conversation_.clear();
  conversation_.set_system_prompt(config_->get_repl_config().system_prompt);
  std::cout << colorize_text("Conversation history cleared.", "green")
            << std::endl;
}

void REPL::handle_history_command() {
  if (conversation_.empty()) {
    std::cout << colorize_text("No conversation history.", "yellow")
              << std::endl;
    return;
  }

  std::cout << colorize_text("Conversation History:", "cyan") << std::endl;
  std::cout << conversation_.to_string() << std::endl;
}

void REPL::handle_save_command(const std::string &filename) {
  try {
    conversation_.save_to_file(config_->expand_path(filename));
    std::cout << colorize_text("Conversation saved to: " + filename, "green")
              << std::endl;
  } catch (const std::exception &e) {
    std::cerr << colorize_text("Error saving conversation: " +
                                   std::string(e.what()),
                               "red")
              << std::endl;
  }
}

void REPL::handle_load_command(const std::string &filename) {
  try {
    conversation_.load_from_file(config_->expand_path(filename));
    std::cout << colorize_text("Conversation loaded from: " + filename, "green")
              << std::endl;
  } catch (const std::exception &e) {
    std::cerr << colorize_text("Error loading conversation: " +
                                   std::string(e.what()),
                               "red")
              << std::endl;
  }
}

void REPL::handle_model_command(const std::string &model_name) {
  llm_service_->set_model(model_name);
  std::cout << colorize_text("Switched to model: " + model_name, "green")
            << std::endl;
}

void REPL::handle_system_command(const std::string &prompt) {
  conversation_.set_system_prompt(prompt);
  std::cout << colorize_text("System prompt updated.", "green") << std::endl;
}

void REPL::handle_exit_command() {
  std::cout << colorize_text("Goodbye!", "cyan") << std::endl;
}

void REPL::load_history() {
  try {
    std::string history_file =
        config_->expand_path(config_->get_repl_config().history_file);
    std::ifstream file(history_file);
    if (file.is_open()) {
      std::string line;
      while (std::getline(file, line)) {
        command_history_.push_back(line);
      }
    }
  } catch (const std::exception &e) {
    std::cout << "[DEBUG] Failed to load history: " << e.what() << "\n";
  }
}

void REPL::save_history() {
  try {
    std::string history_file =
        config_->expand_path(config_->get_repl_config().history_file);
    std::ofstream file(history_file);
    if (file.is_open()) {
      size_t max_history = config_->get_repl_config().max_history;
      size_t start = command_history_.size() > max_history
                         ? command_history_.size() - max_history
                         : 0;

      for (size_t i = start; i < command_history_.size(); ++i) {
        file << command_history_[i] << std::endl;
      }
    }
  } catch (const std::exception &e) {
    std::cout << "[DEBUG] Failed to save history: " << e.what() << "\n";
  }
}

void REPL::add_to_history(const std::string &command) {
  if (!command.empty() &&
      (command_history_.empty() || command_history_.back() != command)) {
    command_history_.push_back(command);
  }
}

void REPL::setup_signal_handlers() {
  std::signal(SIGINT, signal_handler);
  std::signal(SIGTERM, signal_handler);
}

void REPL::cleanup() { save_history(); }

std::string REPL::colorize_text(const std::string &text,
                                const std::string &color) const {
#ifdef _WIN32
  return text;
#else
  std::map<std::string, std::string> colors = {
      {"red", "\033[31m"},   {"green", "\033[32m"},   {"yellow", "\033[33m"},
      {"blue", "\033[34m"},  {"magenta", "\033[35m"}, {"cyan", "\033[36m"},
      {"white", "\033[37m"}, {"reset", "\033[0m"}};

  auto it = colors.find(color);
  if (it != colors.end()) {
    return it->second + text + colors["reset"];
  }
  return text;
#endif
}

void REPL::print_streaming_response(const std::string &prompt) {
  std::cout << colorize_text(config_->get_repl_config().ai_prefix, "green");
  std::cout.flush();

  std::string full_response;
  llm_service_->stream_complete(
      conversation_,
      [this, &full_response](const std::string &chunk, bool is_done) {
        if (!is_done) {
          std::cout << chunk << std::flush;
          full_response += chunk;
        } else {
          std::cout << std::endl << std::endl;
        }
      });

  if (!full_response.empty()) {
    conversation_.add_assistant(full_response);
  }
}

void REPL::print_response(const CompletionResponse &response) {
  if (response.success) {
    std::cout << colorize_text(config_->get_repl_config().ai_prefix, "green")
              << response.content << std::endl
              << std::endl;
  } else {
    std::cerr << colorize_text("Error: " + response.error, "red") << std::endl;
  }
}

void REPL::signal_handler(int signal) {
  if (instance_) {
    std::cout << std::endl
              << instance_->colorize_text(
                     "Interrupt received. Type '/exit' to quit.", "yellow")
              << std::endl;
    instance_->processing_ = false;
  }
}

} // namespace llm