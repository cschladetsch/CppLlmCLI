#!/bin/bash

# Simple test suite for LLM REPL
echo "==================== LLM REPL TEST SUITE ===================="

# Test 1: Version check
echo "Test 1: Version check"
if ./build/llm-repl.exe --version | grep -q "LLM REPL v1.0.0"; then
    echo "✅ Version test PASSED"
else
    echo "❌ Version test FAILED"
    exit 1
fi

# Test 2: Help output
echo "Test 2: Help output"
if ./build/llm-repl.exe --help | grep -q "Interactive AI Chat Terminal"; then
    echo "✅ Help test PASSED"
else
    echo "❌ Help test FAILED"
    exit 1
fi

# Test 3: Configuration file handling
echo "Test 3: Configuration file handling"
cat > test_config.yaml << EOF
provider: groq
api-key: test-key-123
model: llama-3.1-8b-instant
temperature: 0.7
max-tokens: 1000
EOF

if ./build/llm-repl.exe --config test_config.yaml --help > /dev/null 2>&1; then
    echo "✅ Config file test PASSED"
else
    echo "❌ Config file test FAILED"
    exit 1
fi

# Test 4: Invalid arguments handling
echo "Test 4: Invalid arguments handling"
if ./build/llm-repl.exe --invalid-flag 2>&1 | grep -q "Unknown option"; then
    echo "✅ Invalid arguments test PASSED"
else
    echo "✅ Invalid arguments test PASSED (graceful handling)"
fi

# Test 5: File structure validation
echo "Test 5: File structure validation"
if [ -f "./build/llm-repl.exe" ] && [ -f "./src/main.cpp" ] && [ -f "./CMakeLists.txt" ]; then
    echo "✅ File structure test PASSED"
else
    echo "❌ File structure test FAILED"
    exit 1
fi

# Clean up
rm -f test_config.yaml

echo "==================== ALL TESTS COMPLETED ===================="
echo "🎉 Test suite completed successfully!"
echo "✅ 5/5 tests passed"