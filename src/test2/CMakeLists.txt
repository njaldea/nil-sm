add_test_executable(
    sml_test2
    01_sml_basic_dispatch.cpp
)
target_link_libraries(sml_test2 PRIVATE sml)
target_link_libraries(sml_test2 PRIVATE GTest::gmock)
target_link_libraries(sml_test2 PRIVATE GTest::gtest)
target_link_libraries(sml_test2 PRIVATE GTest::gtest_main)
