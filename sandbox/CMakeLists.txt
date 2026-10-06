project(sandbox)

set(ENABLE_SANDBOX  OFF CACHE BOOL "[0 | OFF - 1 | ON]: build sandbox?")
if(NOT ENABLE_SANDBOX)
    return()
endif()

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

project(sandbox_uml)

add_executable(${PROJECT_NAME} uml.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

project(sandbox_barrier_property)

add_executable(${PROJECT_NAME} barrier_property.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

project(sandbox_barrier_shared_ptr)

add_executable(${PROJECT_NAME} barrier_shared_ptr.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

project(sandbox_legacy_style_api)

add_executable(${PROJECT_NAME} legacy_style_api.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

project(sandbox_barrier_state_uml)

add_executable(${PROJECT_NAME} barrier_state_uml.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

project(sandbox_benchmark_1_plain)
add_executable(${PROJECT_NAME} benchmarks/1_plain.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

project(sandbox_benchmark_2_validate)
add_executable(${PROJECT_NAME} benchmarks/2_validate.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

project(sandbox_benchmark_3_ir)
add_executable(${PROJECT_NAME} benchmarks/3_ir.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

project(sandbox_benchmark_4_sm_ir)
add_executable(${PROJECT_NAME} benchmarks/4_sm_ir.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

add_subdirectory(traffic_lights)
add_subdirectory(tollbooth)
add_subdirectory(tollbooth_barrier)
add_subdirectory(viz)