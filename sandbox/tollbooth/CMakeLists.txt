add_executable(sandbox_tollbooth main.cpp)
target_link_libraries(sandbox_tollbooth PRIVATE sm)

add_executable(uml_sandbox_tollbooth uml.cpp)
target_link_libraries(uml_sandbox_tollbooth PRIVATE sm)

if(TARGET sm-viz)
    add_executable(viz_sandbox_tollbooth viz.cpp)
    target_link_libraries(viz_sandbox_tollbooth PRIVATE sm-viz)
endif()