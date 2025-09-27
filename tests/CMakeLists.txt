cmake_minimum_required(VERSION 3.20)

include(GoogleTest)

set(TEST_SOURCES_COMMON
    ../src/llm/groq_service.cpp
    ../src/models/conversation.cpp
    ../src/utils/config.cpp
    ../src/repl/repl.cpp
)

# Use unified HTTP client for all platforms
list(APPEND TEST_SOURCES_COMMON ../src/http/unified_http_client.cpp)

set(TEST_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Test utilities
add_library(test_utils STATIC
    utils/test_helpers.cpp
    utils/test_helpers.hpp
)

target_include_directories(test_utils PUBLIC ${TEST_INCLUDES})
target_link_libraries(test_utils PUBLIC
    gtest
    gmock
    nlohmann_json
    fmt
    spdlog::spdlog
)

# Link httplib only on non-Windows platforms
if(NOT WIN32)
    target_link_libraries(test_utils PUBLIC httplib)
endif()

# Unit Tests
file(GLOB_RECURSE UNIT_TEST_SOURCES "unit/*.cpp")
if(UNIT_TEST_SOURCES)
    add_executable(unit_tests ${UNIT_TEST_SOURCES} ${TEST_SOURCES_COMMON})
    target_include_directories(unit_tests PRIVATE ${TEST_INCLUDES})
    target_link_libraries(unit_tests PRIVATE
        test_utils
        gtest_main
        gmock_main
        nlohmann_json
        CLI11
        fmt
        spdlog::spdlog
        Threads::Threads
    )

    # Link httplib only on non-Windows platforms
    if(NOT WIN32)
        target_link_libraries(unit_tests PRIVATE httplib)
    endif()

    if(WIN32)
        target_link_libraries(unit_tests PRIVATE ws2_32 winhttp)
    endif()

    gtest_discover_tests(unit_tests)
endif()

# Integration Tests
file(GLOB_RECURSE INTEGRATION_TEST_SOURCES "integration/*.cpp")
if(INTEGRATION_TEST_SOURCES)
    add_executable(integration_tests ${INTEGRATION_TEST_SOURCES} ${TEST_SOURCES_COMMON})
    target_include_directories(integration_tests PRIVATE ${TEST_INCLUDES})
    target_link_libraries(integration_tests PRIVATE
        test_utils
        gtest_main
        gmock_main
        nlohmann_json
        CLI11
        fmt
        spdlog::spdlog
        Threads::Threads
    )

    # Link httplib only on non-Windows platforms
    if(NOT WIN32)
        target_link_libraries(integration_tests PRIVATE httplib)
    endif()

    if(WIN32)
        target_link_libraries(integration_tests PRIVATE ws2_32 winhttp)
    endif()

    gtest_discover_tests(integration_tests)
endif()

# Functional Tests
file(GLOB_RECURSE FUNCTIONAL_TEST_SOURCES "functional/*.cpp")
if(FUNCTIONAL_TEST_SOURCES)
    add_executable(functional_tests ${FUNCTIONAL_TEST_SOURCES} ${TEST_SOURCES_COMMON})
    target_include_directories(functional_tests PRIVATE ${TEST_INCLUDES})
    target_link_libraries(functional_tests PRIVATE
        test_utils
        gtest_main
        gmock_main
        nlohmann_json
        CLI11
        fmt
        spdlog::spdlog
        Threads::Threads
    )

    # Link httplib only on non-Windows platforms
    if(NOT WIN32)
        target_link_libraries(functional_tests PRIVATE httplib)
    endif()

    if(WIN32)
        target_link_libraries(functional_tests PRIVATE ws2_32 winhttp)
    endif()

    gtest_discover_tests(functional_tests)
endif()

# Test target that runs all tests
add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS unit_tests integration_tests functional_tests
)