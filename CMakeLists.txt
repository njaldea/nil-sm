cmake_minimum_required(VERSION 3.27.1)

project(
    nil-sm
    VERSION 1.0.1
    LANGUAGES CXX
    DESCRIPTION "template repository for c++ project"
)

find_package(nil-cmake CONFIG REQUIRED)

# defined before the config is generated, which reads it
set(ENABLE_VIZ OFF CACHE BOOL "[0 | OFF - 1 | ON]: build viz?")
install_config("cmake/config.cmake.in")

add_subdirectory(src)
add_subdirectory(sandbox)
