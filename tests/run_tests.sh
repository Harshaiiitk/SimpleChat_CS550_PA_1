#!/bin/bash

# Simple Test Runner for SimpleChat
# Runs the two main test files: basic functionality and integration tests

echo "=========================================="
echo "SimpleChat Test Suite Runner (DSDV/NAT)"
echo "=========================================="

# Get the directory where this script is located
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Change to project root
cd "$PROJECT_ROOT"

# Make test scripts executable
chmod +x tests/*.sh

# Test results tracking
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Function to run a test and track results
run_test() {
    local test_name="$1"
    local test_script="$2"
    shift 2
    local test_args="$@"
    
    echo -e "\n=========================================="
    echo "Running: $test_name"
    echo "=========================================="
    
    ((TOTAL_TESTS++))
    
    if bash "$test_script" $test_args; then
        echo -e "\n✓ PASS: $test_name completed successfully"
        ((PASSED_TESTS++))
    else
        echo -e "\n✗ FAIL: $test_name failed"
        ((FAILED_TESTS++))
    fi
}

# Run the test files
echo "Starting SimpleChat test suite..."

# Test 1: Core Functionality Tests (3 nodes as per requirements)
echo -e "\n=========================================="
echo "Note: Core Functionality Tests (3 nodes)"
echo "=========================================="
echo "To run core functionality tests with 3 nodes (NodeA, NodeB, NodeC):"
echo "  ./test_3nodes.sh"
echo ""

# Test 2: Basic Functionality Tests
run_test "Basic Functionality Tests" "tests/basic_functionality_tests.sh"

# Test 3: Integration Tests
echo -e "\n=========================================="
echo "Running: Integration Tests"
echo "=========================================="

((TOTAL_TESTS++))

if bash tests/integration_tests.sh --test-mode; then
    echo -e "\n✓ PASS: Integration Tests completed successfully"
    ((PASSED_TESTS++))
else
    echo -e "\n✗ FAIL: Integration Tests failed"
    ((FAILED_TESTS++))
fi

# Test 4: NAT Traversal Tests (Linux only)
echo -e "\n=========================================="
echo "Note: NAT Traversal Tests (Linux only)"
echo "=========================================="
echo "To run NAT traversal verification tests:"
echo "  run: sudo ./nat_test.sh"
echo ""

# Final Results Summary
echo -e "\n=========================================="
echo "TEST SUITE RESULTS SUMMARY"
echo "=========================================="
echo "Total Tests Run: $TOTAL_TESTS"
echo "Passed: $PASSED_TESTS"
echo "Failed: $FAILED_TESTS"
echo "Success Rate: $(( (PASSED_TESTS * 100) / TOTAL_TESTS ))%"

if [ $FAILED_TESTS -eq 0 ]; then
    echo -e "\n🎉 ALL TESTS PASSED! 🎉"
    echo "The SimpleChat application is working correctly."
    echo "All basic test cases have been satisfied."
    exit 0
else
    echo -e "\n❌ SOME TESTS FAILED ❌"
    echo "Please review the failed tests above."
    echo "The application may have issues that need to be addressed."
    exit 1
fi
