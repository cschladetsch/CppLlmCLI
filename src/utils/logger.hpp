#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <memory>

namespace llm {

class Logger {
public:
    static void init(bool verbose = false, const std::string& log_file = "") {
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(verbose ? spdlog::level::debug : spdlog::level::info);
        console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

        std::vector<spdlog::sink_ptr> sinks{console_sink};

        if (!log_file.empty()) {
            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file, 1024 * 1024 * 5, 3);
            file_sink->set_level(spdlog::level::debug);
            sinks.push_back(file_sink);
        }

        auto logger = std::make_shared<spdlog::logger>("llm", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::debug);
        logger->flush_on(spdlog::level::warn);

        spdlog::set_default_logger(logger);
        spdlog::set_level(verbose ? spdlog::level::debug : spdlog::level::info);
    }

    static std::shared_ptr<spdlog::logger> get() {
        return spdlog::default_logger();
    }

    static void set_level(spdlog::level::level_enum level) {
        spdlog::set_level(level);
    }

    // Helper function to safely log API keys (shows first/last few chars only)
    static std::string safe_api_key(const std::string& api_key) {
        if (api_key.empty()) {
            return "EMPTY";
        }
        if (api_key.length() <= 8) {
            return std::string(api_key.length(), '*');
        }
        return api_key.substr(0, 4) + "..." + api_key.substr(api_key.length() - 4);
    }
};

} // namespace llm

// Convenience macros
#define LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)
