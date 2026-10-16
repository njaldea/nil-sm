project(viz)

find_package(nil-service CONFIG REQUIRED)

add_library(${PROJECT_NAME} STATIC viewer.cpp)
target_link_libraries(${PROJECT_NAME} PUBLIC sm nil::service)