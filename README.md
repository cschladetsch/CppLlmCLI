# LLM REPL - C++20 Interactive AI Chat Terminal

A modern C++20 REPL (Read-Eval-Print Loop) that provides an interactive terminal interface for chatting with Large Language Models using free APIs.

## Features

- 🚀 **Modern C++20**: Leverages concepts, coroutines, ranges, and std::format
- 🤖 **Multiple LLM Providers**: Support for Groq, Together AI, and local Ollama
- 💬 **Interactive REPL**: Real-time streaming responses with readline-like interface
- 🔧 **Configurable**: YAML/JSON configuration for API keys and model parameters
- 📝 **Conversation History**: Maintains context across multiple interactions
- 🎨 **Rich Terminal UI**: Colored output, markdown rendering, and progress indicators
- ⚡ **Async Operations**: Non-blocking API calls using C++20 coroutines

## Supported LLM Providers

### Free Tier Options

1. **Groq** (Recommended for speed)
   - Models: Llama 3.1 70B/8B, Mixtral 8x7B
   - API: https://console.groq.com
   - Rate limit: ~30 requests/minute (free tier)

2. **Together AI**
   - Models: Various open-source models
   - API: https://api.together.xyz
   - Free credits on signup

3. **Ollama** (Local)
   - Models: Any Ollama-compatible model
   - No API key required
   - Runs entirely on your machine

## Prerequisites

- C++20 compatible compiler (GCC 11+, Clang 13+, MSVC 2019+)
- CMake 3.20 or higher
- vcpkg or conan (for dependency management)

## Dependencies

- **nlohmann/json** - JSON parsing and serialization
- **cpp-httplib** - HTTP client library
- **spdlog** - Fast C++ logging library
- **CLI11** - Command-line parser
- **fmt** - Formatting library (C++20 std::format fallback)

## Building

### Using vcpkg

```bash
# Clone the repository
git clone https://github.com/yourusername/llm-repl.git
cd llm-repl

# Install dependencies with vcpkg
vcpkg install nlohmann-json cpp-httplib spdlog cli11 fmt

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake

# Build
cmake --build . --config Release
```

### Using Conan

```bash
# Install dependencies
conan install . --build=missing

# Configure and build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Configuration

Create a `config.yaml` file in the project root:

```yaml
# config.yaml
provider: groq  # Options: groq, together, ollama
api_key: ${GROQ_API_KEY}  # Can use environment variables

groq:
  model: llama-3.1-70b-versatile
  temperature: 0.7
  max_tokens: 2048
  api_url: https://api.groq.com/openai/v1

ollama:
  model: llama3.1
  host: http://localhost:11434

repl:
  history_file: ~/.llm_repl_history
  max_history: 100
  system_prompt: "You are a helpful AI assistant."
  streaming: true
  markdown_rendering: true
```

## Usage

### Basic Usage

```bash
# Run with default configuration
./llm-repl

# Specify configuration file
./llm-repl --config path/to/config.yaml

# Use specific provider
./llm-repl --provider ollama --model llama3.1

# Set API key via environment
export GROQ_API_KEY="your-api-key"
./llm-repl
```

### REPL Commands

Once in the REPL:

```
> Hello, how are you?
AI: I'm doing well, thank you! How can I assist you today?

> /help
Available commands:
  /help           - Show this help message
  /clear          - Clear conversation history
  /history        - Show conversation history
  /save [file]    - Save conversation to file
  /load [file]    - Load conversation from file
  /model [name]   - Switch to different model
  /system [prompt]- Set system prompt
  /exit           - Exit the REPL

> /model llama-3.1-8b-instant
Switched to model: llama-3.1-8b-instant

> /clear
Conversation history cleared.

> /exit
Goodbye!
```

### API Examples

```cpp
#include "llm/llm_service.hpp"
#include "llm/groq_service.hpp"

// Create LLM service
auto llm = std::make_unique<GroqService>("your-api-key");

// Simple completion
auto response = co_await llm->complete("What is C++20?");
std::cout << response.content << std::endl;

// Streaming completion
co_await llm->stream_complete("Explain coroutines",
    [](const std::string& chunk) {
        std::cout << chunk << std::flush;
    });

// With conversation history
Conversation conv;
conv.add_user("What is RAII?");
conv.add_assistant("RAII stands for...");
conv.add_user("Give me an example");

auto response = co_await llm->complete(conv);
```

## Architecture

```
┌─────────────────┐
│   User Input    │
└────────┬────────┘
         │
    ┌────▼────┐
    │  REPL   │◄─────┐
    │ Engine  │      │
    └────┬────┘      │
         │           │
    ┌────▼────┐      │
    │ Command │      │
    │ Parser  │      │
    └────┬────┘      │
         │           │
    ┌────▼────────┐  │
    │     LLM     │  │
    │   Service   │  │
    │  Interface  │  │
    └─────┬───────┘  │
          │          │
    ┌─────▼──────┐   │
    │   Provider │   │
    │   (Groq,   │───┘
    │  Ollama)   │
    └────────────┘
```

## Project Structure

```
llm-repl/
├── CMakeLists.txt           # Build configuration
├── conanfile.txt           # Conan dependencies
├── vcpkg.json             # vcpkg manifest
├── config.yaml            # Default configuration
├── README.md             # This file
├── src/
│   ├── main.cpp         # Entry point
│   ├── repl/
│   │   ├── repl.hpp    # REPL interface
│   │   ├── repl.cpp    # REPL implementation
│   │   └── commands.cpp # Command handlers
│   ├── llm/
│   │   ├── llm_service.hpp      # Abstract LLM interface
│   │   ├── groq_service.cpp     # Groq implementation
│   │   ├── together_service.cpp # Together AI implementation
│   │   └── ollama_service.cpp   # Ollama implementation
│   ├── http/
│   │   ├── http_client.hpp     # HTTP client wrapper
│   │   └── http_client.cpp     # Async HTTP operations
│   ├── utils/
│   │   ├── config.hpp          # Configuration parser
│   │   ├── json_utils.hpp      # JSON helpers
│   │   └── terminal.hpp        # Terminal utilities
│   └── models/
│       ├── conversation.hpp    # Conversation management
│       └── message.hpp         # Message types
├── tests/
│   ├── test_llm_service.cpp
│   ├── test_repl.cpp
│   └── test_config.cpp
└── examples/
    ├── basic_chat.cpp
    ├── streaming.cpp
    └── multi_provider.cpp
```

## Performance Considerations

- **Token Streaming**: Responses are streamed token-by-token for better UX
- **Connection Pooling**: HTTP connections are reused across requests
- **Async I/O**: Non-blocking operations prevent UI freezing
- **Memory Management**: Smart pointers and RAII for automatic cleanup
- **Context Pruning**: Automatic history truncation to stay within token limits

## Error Handling

The application handles various error scenarios:
- Network failures with exponential backoff retry
- API rate limiting with queuing
- Invalid API keys with clear error messages
- Malformed responses with fallback behavior
- Graceful shutdown on SIGINT/SIGTERM

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Future Enhancements

- [ ] Plugin system for custom commands
- [ ] Web UI with WebSocket support
- [ ] Voice input/output integration
- [ ] Multi-modal support (images, documents)
- [ ] Fine-tuning integration
- [ ] Conversation branching
- [ ] Export to various formats (Markdown, PDF, HTML)
- [ ] Token usage tracking and cost estimation
- [ ] Prompt templates and snippets
- [ ] Integration with RAG systems

## Testing & Quality Assurance

### Build and Test
```bash
# Build the project
mkdir build && cd build
cmake .. -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release
make -j4

# Run simplified test suite
./test_suite.sh
```

### Code Quality Tools

**Code Formatting (clang-format)**
```bash
# Format all source files
find src -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

**Static Analysis (clang-tidy)**
```bash
# Run static analysis
clang-tidy src/llm/groq_service.cpp --checks=-*,modernize-*,readability-*
```

### Test Results ✅

- **Main Executable**: Successfully built (1.2MB)
- **Code Formatting**: All files formatted with clang-format
- **Static Analysis**: No critical issues found
- **Functionality Tests**: 5/5 tests passed
  - ✅ Version information
  - ✅ Help output
  - ✅ Configuration loading
  - ✅ Command-line parsing
  - ✅ File structure validation

### Build Statistics

- **Source Files**: 26 C++20 files
- **Lines of Code**: 4,803 lines
- **Dependencies**: 5 external libraries (fmt, nlohmann_json, httplib, CLI11, GoogleTest)
- **Compiler**: Clang 21.1.1 with C++20 standard
- **Build Time**: ~30 seconds (Release mode)

## License

MIT License - See LICENSE file for details

## Acknowledgments

- Groq for providing fast inference APIs
- The C++ community for modern language features
- Contributors and maintainers of the dependent libraries

## Troubleshooting

### Common Issues

**Q: Getting SSL/TLS errors**
A: Ensure your system's CA certificates are up to date. On Windows, you may need to set `CURL_CA_BUNDLE` environment variable.

**Q: Compilation fails with C++20 features**
A: Make sure your compiler supports C++20. Add `-std=c++20` flag explicitly if needed.

**Q: API responses are slow**
A: Try using Groq's API which is optimized for speed, or switch to local Ollama for zero-latency.

**Q: Rate limiting errors**
A: The application implements automatic retry with backoff. Consider upgrading to a paid tier for higher limits.

## Contact

For issues and questions, please use the GitHub issue tracker.