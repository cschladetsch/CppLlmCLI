#include "utils/logger.hpp"
#include <CLI/CLI.hpp>
#include <iostream>
#include <memory>

#include "llm/groq_service.hpp"
#include "repl/repl.hpp"
#include "utils/config.hpp"

int main(int argc, char *argv[]) {
  // Initialize logger
  llm::Logger::init();

  CLI::App app{"LLM REPL - Interactive AI Chat Terminal"};

  std::string config_file = "config.json";
  std::string provider;
  std::string model;
  std::string api_key;
  float temperature = -1.0f;
  size_t max_tokens = 0;
  bool verbose = false;
  bool version = false;

  app.add_option("-c,--config", config_file, "Configuration file path")
      ->default_val("config.json");

  app.add_option("-p,--provider", provider,
                 "LLM provider (groq, together, ollama)");
  app.add_option("-m,--model", model, "Model to use");
  app.add_option("-k,--api-key", api_key, "API key");
  app.add_option("-t,--temperature", temperature, "Temperature (0.0 - 2.0)");
  app.add_option("--max-tokens", max_tokens, "Maximum tokens to generate");
  app.add_flag("-v,--verbose", verbose, "Enable verbose logging");
  app.add_flag("--version", version, "Show version information");

  CLI11_PARSE(app, argc, argv);

  // Set log level based on verbose flag
  if (verbose) {
    llm::Logger::set_level(spdlog::level::debug);
    LOG_INFO("Verbose logging enabled");
  } else {
    llm::Logger::set_level(spdlog::level::info);
  }

  if (version) {
    std::cout << "LLM REPL v1.0.0" << std::endl;
    std::cout << "A C++20 interactive terminal for Large Language Models"
              << std::endl;
    return 0;
  }

  // Verbose mode - using std::cout instead of spdlog
  if (verbose) {
    std::cout << "[DEBUG] Verbose mode enabled\n";
  }

  try {
    auto config = std::make_unique<llm::Config>(config_file);

    std::map<std::string, std::string> cli_args;
    if (!provider.empty())
      cli_args["provider"] = provider;
    if (!model.empty())
      cli_args["model"] = model;
    if (!api_key.empty())
      cli_args["api-key"] = api_key;
    if (temperature >= 0.0f)
      cli_args["temperature"] = std::to_string(temperature);

    config->merge_command_line_args(cli_args);

    if (config->get_api_key().empty() && config->get_provider() != "ollama") {
      std::cerr << "Error: API key is required for " << config->get_provider()
                << std::endl;
      std::cerr << "Set it via:" << std::endl;
      std::cerr << "  1. Command line: --api-key YOUR_KEY" << std::endl;
      std::cerr << "  2. Environment variable: GROQ_API_KEY, TOGETHER_API_KEY"
                << std::endl;
      std::cerr << "  3. Configuration file: config.yaml" << std::endl;
      return 1;
    }

    auto repl = std::make_unique<llm::REPL>(std::move(config));

    std::cout << "[INFO] Starting LLM REPL...\n";
    repl->run();

  } catch (const std::exception &e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}