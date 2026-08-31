cmake_minimum_required(VERSION 3.16)

project(ToyHive VERSION 0.1.0 LANGUAGES CXX)

add_library(toyhive INTERFACE)
target_include_directories(toyhive
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(toyhive INTERFACE cxx_std_17)

add_executable(toyhive_example main.cpp)
target_link_libraries(toyhive_example PRIVATE toyhive)

option(TOYHIVE_BUILD_TESTS "Build ToyHive boundary tests" OFF)

if(TOYHIVE_BUILD_TESTS)
    enable_testing()

    add_executable(toyhive_boundary_tests tests/hive_boundary_tests.cpp)
    target_link_libraries(toyhive_boundary_tests PRIVATE toyhive)
    add_test(NAME toyhive_boundary_tests COMMAND toyhive_boundary_tests)

    add_executable(toyhive_erase_skipfield_tests tests/hive_erase_skipfield_tests.cpp)
    target_link_libraries(toyhive_erase_skipfield_tests PRIVATE toyhive)
    add_test(NAME toyhive_erase_skipfield_tests COMMAND toyhive_erase_skipfield_tests)

    add_executable(toyhive_copy_move_tests tests/hive_copy_move_tests.cpp)
    target_link_libraries(toyhive_copy_move_tests PRIVATE toyhive)
    add_test(NAME toyhive_copy_move_tests COMMAND toyhive_copy_move_tests)
endif()

if(MSVC)
    target_compile_options(toyhive_example PRIVATE /W4 /permissive-)
    if(TOYHIVE_BUILD_TESTS)
        target_compile_options(toyhive_boundary_tests PRIVATE /W4 /permissive-)
        target_compile_options(toyhive_erase_skipfield_tests PRIVATE /W4 /permissive-)
        target_compile_options(toyhive_copy_move_tests PRIVATE /W4 /permissive-)
    endif()
else()
    target_compile_options(toyhive_example PRIVATE -Wall -Wextra -Wpedantic)
    if(TOYHIVE_BUILD_TESTS)
        target_compile_options(toyhive_boundary_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(toyhive_erase_skipfield_tests PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(toyhive_copy_move_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()