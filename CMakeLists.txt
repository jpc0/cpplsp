cmake_minimum_required(VERSION 3.28)

project(cpplsp LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)
add_subdirectory(tests)
