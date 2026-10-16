add_executable(sandbox_traffic_lights main.cpp)
target_link_libraries(sandbox_traffic_lights PRIVATE sm)

if(TARGET sm-viz)
    add_executable(viz_sandbox_traffic_lights viz.cpp)
    target_link_libraries(viz_sandbox_traffic_lights PRIVATE sm-viz)
endif()