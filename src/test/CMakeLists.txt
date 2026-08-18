add_test_executable(
    sm_test
    test_api.hpp
    01_sm_basic_dispatch.cpp
    02_sm_parent_bubbling.cpp
    03_sm_composite_single_region.cpp
    04_sm_orthogonal.cpp
    05_sm_nested_composite.cpp
    06_sm_transition_semantics.cpp
    07_sm_event_matching.cpp
    08_sm_reaction_validation.cpp
    09_sm_emit_handling.cpp
    10_sm_state_lifetime.cpp
    11_sm_orthogonal_transition_ordering.cpp
    12_sm_edge_cases.cpp
    13_sm_compile_time_diagnostics.cpp
    14_sm_regression_matrix.cpp
    15_sm_state_construction_contexts.cpp
    16_sm_on_regions_complete.cpp
    17_sm_on_enter.cpp
    18_sm_on_exit.cpp
    19_sm_defer_handling.cpp
    20_sm_coalesce_api.cpp
    22_sm_capture_handling.cpp
)
target_link_libraries(sm_test PRIVATE sml)
target_link_libraries(sm_test PRIVATE GTest::gmock)
target_link_libraries(sm_test PRIVATE GTest::gtest)
target_link_libraries(sm_test PRIVATE GTest::gtest_main)
target_compile_options(sm_test PRIVATE -Wno-unused-local-typedefs)

add_test_executable(
    sm_transit_dispatch_state_test
    21_transit_dispatch_state_type_checks.cpp
)
target_link_libraries(sm_transit_dispatch_state_test PRIVATE sml)
target_link_libraries(sm_transit_dispatch_state_test PRIVATE GTest::gmock)
target_link_libraries(sm_transit_dispatch_state_test PRIVATE GTest::gtest)
target_link_libraries(sm_transit_dispatch_state_test PRIVATE GTest::gtest_main)
target_compile_options(sm_transit_dispatch_state_test PRIVATE -Wno-unused-local-typedefs)
