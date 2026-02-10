#!/usr/bin/env bash
#
# T.35 Tool - Basic Metadata Testing Script
# Tests all three injection methods with simple generic metadata
# No JSON parsing required - uses pre-generated binary metadata files
#

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/../.."
TEST_DATA_DIR="${SCRIPT_DIR}"

# Find t35_tool executable (priority order: bin/, mybuild/, build/, find)
find_tool() {
    local tool_path=""
    local source=""

    # 1. Check bin/ directory (SET_CUSTOM_OUTPUT_DIRS=ON)
    if [ -f "${PROJECT_ROOT}/bin/t35_tool" ]; then
        tool_path="${PROJECT_ROOT}/bin/t35_tool"
        source="bin/ (custom output dirs)"
    # 2. Check mybuild/ directory
    elif [ -f "${PROJECT_ROOT}/mybuild/IsoLib/t35_tool/t35_tool" ]; then
        tool_path="${PROJECT_ROOT}/mybuild/IsoLib/t35_tool/t35_tool"
        source="mybuild/ (build directory)"
    # 3. Check build/ directory
    elif [ -f "${PROJECT_ROOT}/build/IsoLib/t35_tool/t35_tool" ]; then
        tool_path="${PROJECT_ROOT}/build/IsoLib/t35_tool/t35_tool"
        source="build/ (build directory)"
    # 4. Search for tool
    else
        tool_path=$(find "${PROJECT_ROOT}" -name "t35_tool" -type f -executable 2>/dev/null | grep -v legacy | head -1)
        if [ -n "$tool_path" ]; then
            source="found at $(dirname "$tool_path")"
        fi
    fi

    if [ -z "$tool_path" ] || [ ! -f "$tool_path" ]; then
        printf "${RED}[ERROR]${NC} t35_tool not found. Please build the project first.\n"
        printf "  Searched locations:\n"
        printf "    - ${PROJECT_ROOT}/bin/t35_tool\n"
        printf "    - ${PROJECT_ROOT}/mybuild/IsoLib/t35_tool/t35_tool\n"
        printf "    - ${PROJECT_ROOT}/build/IsoLib/t35_tool/t35_tool\n"
        exit 1
    fi

    printf "${GREEN}[TOOL]${NC} Using t35_tool from: ${BLUE}%s${NC}\n" "$source"
    printf "       Path: %s\n\n" "$tool_path"

    echo "$tool_path"
}

TOOL=$(find_tool)

# Test configuration
INPUT_VIDEO="${PROJECT_ROOT}/TestData/isobmff/01_simple.mp4"
SOURCE_MANIFEST="${TEST_DATA_DIR}/test_manifest.json"
OUTPUT_DIR="${TEST_DATA_DIR}/output_basic"
T35_PREFIX="B500900001:SMPTE-ST2094-50"

# Injection methods to test
METHODS=(
    "mebx-me4c"
    "dedicated-it35"
    "sample-group"
)

# Test counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Helper functions
log_info() {
    printf "${BLUE}[INFO]${NC} %s\n" "$1"
}

log_success() {
    printf "${GREEN}[PASS]${NC} %s\n" "$1"
}

log_error() {
    printf "${RED}[FAIL]${NC} %s\n" "$1"
}

log_section() {
    printf "\n"
    printf "${BLUE}========================================${NC}\n"
    printf "${BLUE}%s${NC}\n" "$1"
    printf "${BLUE}========================================${NC}\n"
}

# Check prerequisites
check_prerequisites() {
    log_section "Checking Prerequisites"

    if [ ! -f "${INPUT_VIDEO}" ]; then
        log_error "Input video not found: ${INPUT_VIDEO}"
        exit 1
    fi
    log_success "Input video found"

    if [ ! -f "${SOURCE_MANIFEST}" ]; then
        log_error "Test manifest not found: ${SOURCE_MANIFEST}"
        log_info "This script requires simple test data (test_manifest.json and meta_*.bin files)"
        exit 1
    fi
    log_success "Test manifest found"

    # Check for binary metadata files
    local missing=0
    for i in 1 2 3; do
        if [ ! -f "${TEST_DATA_DIR}/meta_00${i}.bin" ]; then
            log_error "Missing metadata file: meta_00${i}.bin"
            missing=$((missing + 1))
        fi
    done

    if [ ${missing} -gt 0 ]; then
        log_error "Missing ${missing} metadata file(s)"
        log_info "Please restore simple test data files"
        exit 1
    fi
    log_success "All metadata files present"

    printf "\n"
}

# Test injection for a single method
test_injection() {
    local method=$1
    local output_file="${OUTPUT_DIR}/${method}_injected.mp4"
    local log_file="${OUTPUT_DIR}/${method}_inject.log"

    log_info "Testing injection method: ${method}"

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if "${TOOL}" inject "${INPUT_VIDEO}" "${output_file}" \
        --source "generic-json:${SOURCE_MANIFEST}" \
        --method "${method}" \
        --t35-prefix "${T35_PREFIX}" \
        > "${log_file}" 2>&1; then

        PASSED_TESTS=$((PASSED_TESTS + 1))
        log_success "Injection successful: ${method}"

        # Get file size
        local size=$(stat -f%z "${output_file}" 2>/dev/null || stat -c%s "${output_file}" 2>/dev/null)
        log_info "Output size: ${size} bytes"
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
        log_error "Injection failed: ${method}"
        log_info "Check log: ${log_file}"
    fi
}

# Test extraction for a single method
test_extraction() {
    local method=$1
    local injected_file="${OUTPUT_DIR}/${method}_injected.mp4"
    local extract_dir="${OUTPUT_DIR}/${method}_extracted"
    local log_file="${OUTPUT_DIR}/${method}_extract.log"

    log_info "Testing extraction from ${method} with auto-detection..."

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if [ ! -f "${injected_file}" ]; then
        log_error "Skipping extraction (injection failed)"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return
    fi

    if "${TOOL}" extract "${injected_file}" "${extract_dir}" \
        --method "auto" \
        --t35-prefix "${T35_PREFIX}" \
        > "${log_file}" 2>&1; then

        PASSED_TESTS=$((PASSED_TESTS + 1))
        log_success "Extraction successful"

        # Count extracted files
        local count=$(ls -1 "${extract_dir}"/metadata_*.bin 2>/dev/null | wc -l | tr -d ' ')
        log_info "Extracted ${count} metadata files"
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
        log_error "Extraction failed"
        log_info "Check log: ${log_file}"
    fi
}

# Verify round-trip integrity
verify_roundtrip() {
    local method=$1
    local extract_dir="${OUTPUT_DIR}/${method}_extracted"

    log_info "Verifying round-trip integrity for ${method}..."

    TOTAL_TESTS=$((TOTAL_TESTS + 1))

    if [ ! -d "${extract_dir}" ]; then
        log_error "Skipping verification (extraction failed)"
        FAILED_TESTS=$((FAILED_TESTS + 1))
        return
    fi

    local all_match=true
    for i in 1 2 3; do
        local original="${TEST_DATA_DIR}/meta_00${i}.bin"
        local extracted="${extract_dir}/metadata_${i}.bin"

        if [ ! -f "${extracted}" ]; then
            log_error "Missing extracted file: metadata_${i}.bin"
            all_match=false
            continue
        fi

        if ! diff -q "${original}" "${extracted}" > /dev/null 2>&1; then
            log_error "Mismatch: metadata_${i}.bin"
            all_match=false
        fi
    done

    if [ "$all_match" = true ]; then
        PASSED_TESTS=$((PASSED_TESTS + 1))
        log_success "Round-trip verified: all files match"
    else
        FAILED_TESTS=$((FAILED_TESTS + 1))
        log_error "Round-trip verification failed"
    fi
}

# Main execution
main() {
    log_section "T.35 Tool - Basic Metadata Tests"

    log_info "Test configuration:"
    log_info "  Input video: ${INPUT_VIDEO}"
    log_info "  Source manifest: ${SOURCE_MANIFEST}"
    log_info "  Output directory: ${OUTPUT_DIR}"
    log_info "  T.35 prefix: ${T35_PREFIX}"
    printf "\n"

    # Setup
    mkdir -p "${OUTPUT_DIR}"
    check_prerequisites

    # Test all methods: inject, extract, verify
    for method in "${METHODS[@]}"; do
        log_section "Testing Method: ${method}"

        test_injection "${method}"
        printf "\n"

        test_extraction "${method}"
        printf "\n"

        verify_roundtrip "${method}"
        printf "\n"
    done

    # Summary
    log_section "Test Summary"
    log_info "Total tests: ${TOTAL_TESTS}"
    log_info "Passed: ${PASSED_TESTS}"
    log_info "Failed: ${FAILED_TESTS}"
    printf "\n"

    if [ ${FAILED_TESTS} -eq 0 ]; then
        log_success "All tests passed!"
        exit 0
    else
        log_error "Some tests failed"
        log_info "Check logs in: ${OUTPUT_DIR}"
        exit 1
    fi
}

# Run main
main
