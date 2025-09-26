# External dependencies setup
include(FetchContent)
include(ExternalProject)

# Set download directory
set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/downloads CACHE PATH "External dependencies path" FORCE)

# Google Test
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# nlohmann_json
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

# cpp-httplib (not needed on Windows since we use WinHTTP)
if(NOT WIN32)
  FetchContent_Declare(
    httplib
    URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.14.3.zip
  )
endif()
# CLI11
FetchContent_Declare(
  CLI11
  URL https://github.com/CLIUtils/CLI11/archive/refs/tags/v2.4.1.zip
)

# fmt
FetchContent_Declare(
  fmt
  URL https://github.com/fmtlib/fmt/releases/download/9.1.0/fmt-9.1.0.zip
)

# Add spdlog submodule
add_subdirectory(spdlog)

# Make dependencies available
if(WIN32)
  FetchContent_MakeAvailable(googletest nlohmann_json CLI11 fmt)
else()
  FetchContent_MakeAvailable(googletest nlohmann_json httplib CLI11 fmt)
endif()
