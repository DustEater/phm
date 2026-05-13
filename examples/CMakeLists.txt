cmake_minimum_required(VERSION 3.16)
project(phm_examples LANGUAGES CXX)

add_executable(phm_example_basic basic_usage.cpp)
target_link_libraries(phm_example_basic PRIVATE phm)
target_include_directories(phm_example_basic PRIVATE ${CMAKE_SOURCE_DIR}/include)