cmake_minimum_required(VERSION 3.20)
project(llm-repl VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

# Include external dependencies
add_subdirectory(external)

find_package(Threads REQUIRED)

if(WIN32)
    find_package(OpenSSL QUIET)
endif()

set(SOURCES
    src/main.cpp
    src/repl/repl.cpp
    src/llm/groq_service.cpp
    src/http/http_client.cpp
    src/utils/config.cpp
    src/models/conversation.cpp
)

set(HEADERS
    src/repl/repl.hpp
    src/llm/llm_service.hpp
    src/llm/groq_service.hpp
    src/http/http_client.hpp
    src/utils/config.hpp
    src/models/conversation.hpp
    src/models/message.hpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    nlohmann_json
    httplib
    CLI11
    fmt
    Threads::Threads
)

if(WIN32)
    if(OpenSSL_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE
            OpenSSL::SSL
            OpenSSL::Crypto
        )
    endif()
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fcoroutines)
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

install(FILES config.yaml
    DESTINATION etc/${PROJECT_NAME}
)

# Temporarily disable tests for formatting and linting
# enable_testing()
# Add GTest dependency
# include(GoogleTest)
# Include test directory
# add_subdirectory(tests)

add_custom_target(format
    COMMAND clang-format -i ${SOURCES} ${HEADERS}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(run
    COMMAND $<TARGET_FILE:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)