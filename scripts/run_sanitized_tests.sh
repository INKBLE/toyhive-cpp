#!/usr/bin/env bash
set -Eeuo pipefail

readonly project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly build_dir="${project_root}/build-sanitized"

cmake -S "${project_root}" -B "${build_dir}" \
    -DTOYHIVE_BUILD_TESTS=ON \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build "${build_dir}" --parallel

readonly test_binary="${build_dir}/toyhive_boundary_tests"
readonly erase_test_binary="${build_dir}/toyhive_erase_skipfield_tests"
readonly copy_move_test_binary="${build_dir}/toyhive_copy_move_tests"
readonly exception_safety_test_binary="${build_dir}/toyhive_exception_safety_tests"
status=0
for test_name in \
    empty_container \
    insertion_boundaries_and_size \
    non_trivial_object_lifetime; do
    echo "=== ${test_name} ==="
    if ! ASAN_OPTIONS="detect_leaks=1:halt_on_error=0" \
        UBSAN_OPTIONS="halt_on_error=0:print_stacktrace=1" \
        "${test_binary}" "${test_name}"; then
        status=1
    fi
done

for test_name in \
    emplace_failure_in_existing_block_preserves_state \
    emplace_failure_in_new_block_preserves_state \
    copy_constructor_failure_releases_partial_copy \
    copy_assignment_failure_preserves_target; do
    echo "=== ${test_name} ==="
    if ! ASAN_OPTIONS="detect_leaks=1:halt_on_error=0" \
        UBSAN_OPTIONS="halt_on_error=0:print_stacktrace=1" \
        "${exception_safety_test_binary}" "${test_name}"; then
        status=1
    fi
done

for test_name in \
    copy_constructor_preserves_values_and_holes \
    copy_assignment_replaces_old_values \
    move_constructor_transfers_storage \
    move_assignment_releases_target_and_transfers_storage \
    self_assignment_and_self_move_are_safe \
    non_trivial_copy_move_lifetime; do
    echo "=== ${test_name} ==="
    if ! ASAN_OPTIONS="detect_leaks=1:halt_on_error=0" \
        UBSAN_OPTIONS="halt_on_error=0:print_stacktrace=1" \
        "${copy_move_test_binary}" "${test_name}"; then
        status=1
    fi
done

for test_name in \
    single_hole_merge_and_erase_return \
    left_hole_merge \
    right_hole_merge \
    bidirectional_hole_merge \
    alternating_holes_bidirectional_iteration \
    erase_entire_middle_block_and_cross_it \
    erase_range_within_block \
    erase_range_across_blocks_and_existing_holes \
    erase_range_to_end_and_empty_range; do
    echo "=== ${test_name} ==="
    if ! ASAN_OPTIONS="detect_leaks=1:halt_on_error=0" \
        UBSAN_OPTIONS="halt_on_error=0:print_stacktrace=1" \
        "${erase_test_binary}" "${test_name}"; then
        status=1
    fi
done

echo "=== public_iterator_and_erase_api_compile_check ==="
readonly compile_log="${build_dir}/hive_api_compile_check.log"
if g++ -std=c++17 -Wall -Wextra -Wpedantic -I"${project_root}" \
    "${project_root}/tests/hive_api_compile_check.cpp" \
    -o "${build_dir}/hive_api_compile_check" >"${compile_log}" 2>&1; then
    echo "[PASS] Public iterator and erase APIs compile."
else
    status=1
    echo "[FAIL] Public iterator and erase APIs do not compile."
    cat "${compile_log}"
fi

exit "${status}"