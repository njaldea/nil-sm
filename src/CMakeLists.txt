project(sml)

find_package(nil-xalt CONFIG REQUIRED)
set(
    HEADERS
        publish/nil/sml.hpp
)

add_library(${PROJECT_NAME} INTERFACE ${HEADERS})
target_link_libraries(${PROJECT_NAME} INTERFACE nil::xalt)

nil_install_headers(${PROJECT_NAME} INTERFACE)
nil_install_targets(${PROJECT_NAME})

add_test_subdirectory()