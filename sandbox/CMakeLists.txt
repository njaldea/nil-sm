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

project(sandbox_tollbooth)

add_executable(${PROJECT_NAME} tollbooth/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

project(sandbox_tollbooth_uml)

add_executable(${PROJECT_NAME} tollbooth/uml.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

# tollbooth_barrier - one machine, jobs held behind nil::sm::barrier::State.
foreach(JOB startup collection shift maintenance)
    project(tollbooth_barrier_job_${JOB})

    add_library(
        ${PROJECT_NAME}
        STATIC
        tollbooth_barrier/${JOB}.cpp
        tollbooth_barrier/${JOB}.hpp
    )
    target_link_libraries(${PROJECT_NAME} PUBLIC sm)
endforeach()

project(sandbox_tollbooth_barrier)

add_executable(${PROJECT_NAME} tollbooth_barrier/main.cpp)
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE sm
            tollbooth_barrier_job_startup
            tollbooth_barrier_job_collection
            tollbooth_barrier_job_shift
            tollbooth_barrier_job_maintenance
)

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

project(sandbox_tollbooth_barrier_uml)

add_executable(${PROJECT_NAME} tollbooth_barrier/uml.cpp)
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE sm
            tollbooth_barrier_job_startup
            tollbooth_barrier_job_collection
            tollbooth_barrier_job_shift
            tollbooth_barrier_job_maintenance
)