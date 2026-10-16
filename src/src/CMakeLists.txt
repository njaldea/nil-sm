find_package(nil-service CONFIG REQUIRED)

set(ASSETS index_html index_js)
set(index_html_FILE assets/index.html)
set(index_js_FILE assets/index.js)

set(GENERATED)
foreach(ASSET IN LISTS ASSETS)
    set(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/${ASSET}.cpp)
    add_custom_command(
        OUTPUT ${OUTPUT}
        COMMAND ${CMAKE_COMMAND}
            -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/${${ASSET}_FILE}
            -DOUTPUT=${OUTPUT}
            -DNAME=${ASSET}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/embed.cmake
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${${ASSET}_FILE} embed.cmake
    )
    set_source_files_properties(${OUTPUT} PROPERTIES SKIP_LINTING ON)
    list(APPEND GENERATED ${OUTPUT})
endforeach()

add_library(${PROJECT_NAME}-viz STATIC viz.cpp ${GENERATED})
set_target_properties(${PROJECT_NAME}-viz PROPERTIES OUTPUT_NAME "nil-${PROJECT_NAME}-viz")
target_link_libraries(${PROJECT_NAME}-viz PRIVATE nil::service)
target_link_libraries(${PROJECT_NAME}-viz PUBLIC sm)

nil_install_targets(${PROJECT_NAME}-viz)