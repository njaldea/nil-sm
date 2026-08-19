project(sml)

find_package(nil-xalt CONFIG REQUIRED)
set(
    HEADERS
        publish/nil/sm.hpp
        publish/nil/sm/api.hpp
        publish/nil/sm/concepts.hpp
        publish/nil/sm/detail.hpp
        publish/nil/sm/state.hpp
        publish/nil/sm/structs.hpp
)

add_library(${PROJECT_NAME} INTERFACE ${HEADERS})
target_link_libraries(${PROJECT_NAME} INTERFACE nil::xalt)

nil_install_headers(${PROJECT_NAME} INTERFACE)
nil_install_targets(${PROJECT_NAME})

add_test_subdirectory()