# Copyright(c) 2019 spdlog authors Distributed under the MIT License (http://opensource.org/licenses/MIT)

#cmake_minimum_required(VERSION 3.10)
#project(spdlog_examples CXX)


find_package(spdlog REQUIRED)
#include_directories(${SPDLOG_INCLUDE_DIR})
#message("spdlog include dir: " ${SPDLOG_INCLUDE_DIR})

#add_executable(example example.cpp)
#target_link_libraries(example PRIVATE spdlog::spdlog)

add_executable(example_header_only example.cpp)
target_link_libraries(example_header_only spdlog::spdlog_header_only)