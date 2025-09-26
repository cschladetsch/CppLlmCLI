#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "llm/llm_service.hpp"
#include "models/conversation.hpp"
#include "utils/config.hpp"

namespace llm {

class REPL {
public:
  explicit REPL(std::unique_ptr<Config> config);
  ~REPL();

  void run();
  void stop();

  void set_llm_service(std::unique_ptr<LLMService> service);

private:
  std::unique_ptr<Config> config_;
  std::unique_ptr<LLMService> llm_service_;
  Conversation conversation_;
  std::atomic<bool> running_{false};
  std::atomic<bool> processing_{false};

  std::vector<std::string> command_history_;
  size_t history_index_ = 0;

  void print_welcome();
  void print_help();
  std::string read_input();
  bool process_command(const std::string &input);
  void process_user_input(const std::string &input);

  bool handle_slash_command(const std::string &command);
  void handle_help_command();
  void handle_clear_command();
  void handle_history_command();
  void handle_save_command(const std::string &filename);
  void handle_load_command(const std::string &filename);
  void handle_model_command(const std::string &model_name);
  void handle_system_command(const std::string &prompt);
  void handle_exit_command();

  void load_history();
  void save_history();
  void add_to_history(const std::string &command);

  void setup_signal_handlers();
  void cleanup();

  std::string colorize_text(const std::string &text,
                            const std::string &color) const;
  void print_streaming_response(const std::string &prompt);
  void print_response(const CompletionResponse &response);

  static void signal_handler(int signal);
  static REPL *instance_;
};

} // namespace llm