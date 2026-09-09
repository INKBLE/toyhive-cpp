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

ASAN_OPTIONS="detect_leaks=1:halt_on_error=0" \
    UBSAN_OPTIONS="halt_on_error=0:print_stacktrace=1" \
    ctest --test-dir "${build_dir}" --output-on-failure
