cmake_minimum_required(VERSION 3.27.1)

project(
    nil-sm
    VERSION 0.0.1
    LANGUAGES CXX
    DESCRIPTION "template repository for c++ project"
)

find_package(nil-cmake CONFIG REQUIRED)
install_config("cmake/config.cmake.in")

add_subdirectory(src)
add_subdirectory(sandbox)
