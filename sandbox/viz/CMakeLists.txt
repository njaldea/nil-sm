project(viz)

find_package(nil-xit CONFIG REQUIRED)

add_executable(${PROJECT_NAME} main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sm)
target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        tollbooth_barrier_job_startup
        tollbooth_barrier_job_collection
        tollbooth_barrier_job_shift
        tollbooth_barrier_job_maintenance
)
target_link_libraries(${PROJECT_NAME} PRIVATE nil::xit)