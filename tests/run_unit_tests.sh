#!/bin/bash
# Run all unit tests with improved output formatting

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "${BOLD}${BLUE}Building tests...${NC}"
make

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo -e "\n${BOLD}${BLUE}Running unit tests...${NC}\n"

# Test binaries
TESTS=(
    "./test_linked_list"
    "./test_preprocessing"
    "./test_job_receiver"
    "./test_simulation_stats"
    "./test_timed_queue"
)

TOTAL_PASSED=0
TOTAL_FAILED=0
SUITES_RUN=0
FAILED_SUITES=()

# Run each test
for test in "${TESTS[@]}"; do
    if [ ! -f "$test" ]; then
        echo -e "${YELLOW}⚠ Warning: $test not found (skipping)${NC}"
        continue
    fi
    
    # Run test and capture output
    OUTPUT=$($test 2>&1)
    TEST_EXIT=$?
    echo "$OUTPUT"
    
    # Parse results from output
    PASSED=$(echo "$OUTPUT" | grep -oE "[0-9]+ passed" | grep -oE "[0-9]+" | head -1 || echo "0")
    FAILED=$(echo "$OUTPUT" | grep -oE "[0-9]+ failed" | grep -oE "[0-9]+" | head -1 || echo "0")
    
    if [ -n "$PASSED" ]; then
        TOTAL_PASSED=$((TOTAL_PASSED + PASSED))
    fi
    
    if [ -n "$FAILED" ]; then
        TOTAL_FAILED=$((TOTAL_FAILED + FAILED))
        if [ "$FAILED" -gt 0 ]; then
            FAILED_SUITES+=("$test")
        fi
    fi
    
    SUITES_RUN=$((SUITES_RUN + 1))
done

# Print summary
echo ""
echo -e "${BOLD}================================================================${NC}"
echo -e "${BOLD}${BLUE}OVERALL TEST SUMMARY${NC}"
echo -e "${BOLD}Test Suites: ${BLUE}${SUITES_RUN}${NC}"

if [ $TOTAL_FAILED -eq 0 ]; then
    echo -e "${BOLD}${GREEN}✓ ALL TESTS PASSED${NC}"
    echo -e "${BOLD}Total: ${GREEN}${TOTAL_PASSED} passed${NC}"
else
    echo -e "${BOLD}${RED}✗ SOME TESTS FAILED${NC}"
    echo -e "${BOLD}Total: ${GREEN}${TOTAL_PASSED} passed${NC}, ${RED}${TOTAL_FAILED} failed${NC}"
    if [ ${#FAILED_SUITES[@]} -gt 0 ]; then
        echo -e "\n${RED}Failed suites:${NC}"
        for suite in "${FAILED_SUITES[@]}"; do
            echo -e "  ${RED}✗ $suite${NC}"
        done
    fi
fi

echo -e "${BOLD}================================================================${NC}\n"

# Cleanup
echo -e "${BOLD}${BLUE}Cleaning up...${NC}"
make clean

# Exit with failure if any tests failed
if [ $TOTAL_FAILED -gt 0 ]; then
    exit 1
fi

exit 0