add_test_executable(
    sml_test
    01_sml_basic_dispatch.cpp
    02_sml_parent_bubbling.cpp
    03_sml_composite_single_region.cpp
    04_sml_orthogonal.cpp
    05_sml_nested_composite.cpp
    06_sml_transition_semantics.cpp
    07_sml_event_matching.cpp
    08_sml_reaction_validation.cpp
    09_sml_emit_handling.cpp
    10_sml_state_lifetime.cpp
    11_sml_orthogonal_transition_ordering.cpp
    12_sml_edge_cases.cpp
    13_sml_compile_time_diagnostics.cpp
    14_sml_regression_matrix.cpp
    15_sml_state_construction_contexts.cpp
    16_sml_on_regions_complete.cpp
    17_sml_on_enter.cpp
    18_sml_on_exit.cpp
)
target_link_libraries(sml_test PRIVATE sml)
target_link_libraries(sml_test PRIVATE GTest::gmock)
target_link_libraries(sml_test PRIVATE GTest::gtest)
target_link_libraries(sml_test PRIVATE GTest::gtest_main)
