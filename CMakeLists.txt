cmake_minimum_required(VERSION 3.27.1)

project(
    nil-template CXX
    VERSION 0.0.0
    LANGUAGES CXX
    DESCRIPTION "template repository for c++ project"
)

find_package(nil-cmake CONFIG REQUIRED)
install_config("cmake/config.cmake.in")

add_subdirectory(src)
