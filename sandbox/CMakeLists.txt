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

project(sandbox_toll_uml)

add_executable(${PROJECT_NAME} tollbooth/uml.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)

# Same machine, three assemblies, kept side by side so their build cost can be compared:
#   tollbooth       - one machine, everything instantiated in a single translation unit
#   tollbooth_libs  - one machine per job behind nil::sm::ISM factories, one library each
#   tollbooth_slot  - one machine, jobs held as opaque child machines via job_slot
foreach(JOB startup collection shift maintenance)
    project(toll_job_${JOB})

    add_library(${PROJECT_NAME} STATIC tollbooth_libs/${JOB}.cpp)
    target_link_libraries(${PROJECT_NAME} PUBLIC sm)
endforeach()

project(sandbox_tollbooth_libs)

add_executable(${PROJECT_NAME} tollbooth_libs/main.cpp)
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE sm
            toll_job_startup
            toll_job_collection
            toll_job_shift
            toll_job_maintenance
)

project(sandbox_tollbooth_slot)

add_executable(${PROJECT_NAME} tollbooth_slot/main.cpp)
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE sm
            toll_job_startup
            toll_job_collection
            toll_job_shift
            toll_job_maintenance
)

# tollbooth_barrier - one machine, jobs held behind nil::sm::barrier::State, each job's
# barrier::SM built in its own library sharing the host's Runtime.
foreach(JOB startup collection shift maintenance)
    project(toll_barrier_job_${JOB})

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
            toll_barrier_job_startup
            toll_barrier_job_collection
            toll_barrier_job_shift
            toll_barrier_job_maintenance
)

project(sandbox_toll_barrier_uml)

add_executable(${PROJECT_NAME} tollbooth_barrier/uml.cpp)
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE sm
            toll_barrier_job_startup
            toll_barrier_job_collection
            toll_barrier_job_shift
            toll_barrier_job_maintenance
)