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