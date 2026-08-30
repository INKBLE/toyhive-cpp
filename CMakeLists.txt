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

if(MSVC)
    target_compile_options(toyhive_example PRIVATE /W4 /permissive-)
else()
    target_compile_options(toyhive_example PRIVATE -Wall -Wextra -Wpedantic)
endif()