add_executable(sandbox_tollbooth main.cpp)
target_link_libraries(sandbox_tollbooth PRIVATE sm)

add_executable(sandbox_tollbooth_uml uml.cpp)
target_link_libraries(sandbox_tollbooth_uml PRIVATE sm)