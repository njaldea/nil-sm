# One machine, with jobs held behind nil::sm::barrier::State.
foreach(JOB startup collection shift maintenance)
    set(TARGET tollbooth_barrier_job_${JOB})
    add_library(
        ${TARGET}
        STATIC
        ${JOB}.cpp
        ${JOB}.hpp
    )
    target_link_libraries(${TARGET} PUBLIC sm)
endforeach()

set(TOLLBOOTH_BARRIER_JOBS
    tollbooth_barrier_job_startup
    tollbooth_barrier_job_collection
    tollbooth_barrier_job_shift
    tollbooth_barrier_job_maintenance
)

add_executable(sandbox_tollbooth_barrier main.cpp)
target_link_libraries(sandbox_tollbooth_barrier PRIVATE sm ${TOLLBOOTH_BARRIER_JOBS})

add_executable(sandbox_tollbooth_barrier_uml uml.cpp)
target_link_libraries(sandbox_tollbooth_barrier_uml PRIVATE sm ${TOLLBOOTH_BARRIER_JOBS})