# Test Coverage

| Test | Description |
|------|-------------|
| [leaf_event_forwarded](01_sm_basic_dispatch.cpp#L83) | Leaf returns `Forward`; event dispatched and state exits on SM destroy |
| [leaf_event_discarded](01_sm_basic_dispatch.cpp#L99) | Leaf returns `Discard`; event consumed, no bubbling |
| [leaf_event_unhandled](01_sm_basic_dispatch.cpp#L115) | Leaf has no events list; incoming event is silently ignored |
| [leaf_event_terminate](01_sm_basic_dispatch.cpp#L130) | Leaf returns `Terminate`; region is nulled immediately, SM destructor has nothing to clean up |
| [leaf_event_emit](01_sm_basic_dispatch.cpp#L146) | Leaf returns `Emit<e2>`; emitted event enqueued and drained; state stays alive |
| [leaf_event_transit](01_sm_basic_dispatch.cpp#L163) | Leaf returns `Transit<T>`; source exits and target is created in place |
| [state_ignores_unregistered_event](01_sm_basic_dispatch.cpp#L183) | State has an events list that doesn't include `e2`; `e2` is silently dropped |
| [parent_handles_forwarded_event](02_sm_parent_bubbling.cpp#L8) | Child forwards → parent handles and discards (stops bubbling) |
| [parent_forwards_forwarded_event](02_sm_parent_bubbling.cpp#L52) | Child forwards → parent forwards → grandparent handles (3-level bubble chain) |
| [parent_skips_discarded_child_event](02_sm_parent_bubbling.cpp#L111) | Child discards → parent `on_event` is never called |
| [parent_handles_unhandled_child_event](02_sm_parent_bubbling.cpp#L154) | Child has no event handler → parent handles event directly |
| [child_terminate_does_not_bubble_to_parent](02_sm_parent_bubbling.cpp#L193) | Child terminates (`Terminate ≠ Forward`); parent `on_event` skipped; `on_regions_finalized` triggered |
| [no_parent_handler_on_unhandled_event](02_sm_parent_bubbling.cpp#L238) | Neither child nor parent handle the event; nothing happens |
| [parent_transits_after_child_forward](02_sm_parent_bubbling.cpp#L271) | Child forwards → parent transits to `NewState`; child destroyed inside parent destructor |
| [parent_terminates_after_child_forward](02_sm_parent_bubbling.cpp#L324) | Child forwards → parent terminates itself; both destroyed during event processing |
| [parent_emits_after_child_forward](02_sm_parent_bubbling.cpp#L369) | Child forwards → parent emits `e2`; emitted event dispatched in next SM cycle |
| [parent_handles_event_unregistered_in_child](02_sm_parent_bubbling.cpp#L420) | Child has events for `e2` only; `e1` treated as unhandled → parent handles directly |
| [child_transit_does_not_bubble_to_parent](02_sm_parent_bubbling.cpp#L465) | Child transits (`Transit ≠ Forward`); parent `on_event` never called |
| [child_transition_applies_when_parent_no_transition](03_sm_composite_single_region.cpp#L8) | Child transits on first event; after transition, child forwards on second event and parent handles |
| [composite_receives_child_forward](03_sm_composite_single_region.cpp#L71) | Three-level chain: leaf → mid → root, all via `Forward` |
| [parent_transition_after_child_forward](03_sm_composite_single_region.cpp#L129) | Child forwards → parent transits to `ParentTarget`; child destroyed along with parent |
| [all_discard_parent_skipped](04_sm_orthogonal.cpp#L8) | Both orthogonal regions discard; parent `on_event` never called |
| [forward_discard_parent_handles](04_sm_orthogonal.cpp#L65) | One region forwards, one discards; any `Forward` is enough to call parent |
| [all_unhandled_parent_handles](04_sm_orthogonal.cpp#L123) | All regions unhandled (`all_unhandled = true`); parent handles event directly |
| [no_parent_all_unhandled](04_sm_orthogonal.cpp#L169) | All regions unhandled and parent has no events; event is silently dropped |
| [both_regions_forward_parent_handles](04_sm_orthogonal.cpp#L209) | Both regions forward; any `Forward` is sufficient to trigger parent |
| [one_forward_one_unhandled_parent_handles](04_sm_orthogonal.cpp#L267) | One region forwards, one is unhandled; `Forward` alone triggers parent |
| [both_regions_terminate_triggers_regions_complete](04_sm_orthogonal.cpp#L319) | Both regions terminate in same event; all regions null → `on_regions_finalized` fired |
| [one_branch_discards_other_forwards_asymmetric](05_sm_nested_composite.cpp#L8) | One branch discards (parent skipped), other branch forwards (parent and root called) |
| [both_branches_discard_parent_skipped](05_sm_nested_composite.cpp#L96) | Both nested branches discard; no forward propagates up, root never called |
| [both_branches_forward_parent_handles](05_sm_nested_composite.cpp#L181) | Both nested branches forward; both branch parents called then root called |
| [self_transition_reconstructs_state](06_sm_transition_semantics.cpp#L33) | State transits to itself; old instance exits and new instance enters |
| [transition_to_sibling_state](06_sm_transition_semantics.cpp#L62) | State transits to a sibling; source exits, target created, responds to next event |
| [transition_to_parents_child](06_sm_transition_semantics.cpp#L118) | Child of mid transits to another child of mid; mid stays alive |
| [transition_destroys_previous_state](06_sm_transition_semantics.cpp#L183) | Transit destroys source and creates target; target exits on SM destroy |
| [multiple_consecutive_transitions](06_sm_transition_semantics.cpp#L220) | Three sequential transitions; each state exits before the next is created |
| [transition_after_forwarded_event](06_sm_transition_semantics.cpp#L287) | Child forwards → parent transits; transit applied after bubbling completes |
| [transition_after_discarded_event](06_sm_transition_semantics.cpp#L366) | Child discards → parent still transits (parent handles its own event independently) |
| [event_matches_first_handler](07_sm_event_matching.cpp#L8) | State with two event handlers; `e1` dispatches to the first one |
| [event_matches_last_handler](07_sm_event_matching.cpp#L41) | State with two event handlers; `e2` dispatches to the second one |
| [event_matches_none](07_sm_event_matching.cpp#L74) | Event `e3` not in state's events list; no handler called |
| [state_with_empty_event_list](07_sm_event_matching.cpp#L107) | Explicit `events = tlist<>`; all events are silently ignored |
| [state_with_default_events_only](07_sm_event_matching.cpp#L130) | No `events` member; coalesces to empty, all events silently ignored |
| [static_concept_checks_compile](08_sm_reaction_validation.cpp#L97) | Compile-time `static_assert` checks that valid/invalid return types satisfy `has_on_event` concept |
| [emit_from_leaf_reaction](09_sm_emit_handling.cpp#L155) | Leaf emits `out_a`; sink state in same SM receives it |
| [emit_during_forward_path](09_sm_emit_handling.cpp#L167) | Leaf emits or forwards based on payload; emitted event delivered before next forward |
| [emit_from_parent_reaction](09_sm_emit_handling.cpp#L184) | Parent emits `out_b` when handling unhandled child event; sink receives it |
| [emit_from_orthogonal_regions](09_sm_emit_handling.cpp#L196) | Both orthogonal regions emit; both arrive at sink in region order |
| [multiple_emit_events_preserve_order](09_sm_emit_handling.cpp#L228) | Sequential emits from same state; sink receives them in FIFO order |
| [state_constructor_called_once](10_sm_state_lifetime.cpp#L8) | State is constructed once at SM creation and destroyed at SM destruction |
| [transition_destroys_previous_and_creates_new_instance](10_sm_state_lifetime.cpp#L36) | Transition destroys source, creates target; target responds to next event then exits |
| [orthogonal_region_destruction](10_sm_state_lifetime.cpp#L84) | Two orthogonal regions each constructed once; destroyed in reverse order |
| [parent_destruction_destroys_all_children](10_sm_state_lifetime.cpp#L110) | Nested hierarchy: grandchild exits before child before root on SM destroy |
| [child_transition_plus_parent_discard](11_sm_orthogonal_transition_ordering.cpp#L8) | Child transits, another region forwards → parent discards; child transit applied |
| [child_transition_plus_parent_forward](11_sm_orthogonal_transition_ordering.cpp#L75) | Child transits, another forwards → parent forwards to grandparent; sub-transit applied before bubbling |
| [child_transition_plus_parent_transition](11_sm_orthogonal_transition_ordering.cpp#L159) | Child transits, another forwards → parent also transits; parent transit cancels pending child transit |
| [multiple_child_transitions](11_sm_orthogonal_transition_ordering.cpp#L231) | Both orthogonal regions transit; both transitions applied independently |
| [nested_child_transitions](11_sm_orthogonal_transition_ordering.cpp#L307) | Deeply nested child transits; only the immediate region's state is replaced |
| [sibling_transitions_independent](11_sm_orthogonal_transition_ordering.cpp#L379) | Two sibling regions each transit to different targets; neither affects the other |
| [parent_transition_cancels_pending_child_transitions](11_sm_orthogonal_transition_ordering.cpp#L455) | Parent transits while child also has a pending transit; parent wins, child transit is discarded |
| [state_with_no_regions](12_sm_edge_cases.cpp#L202) | State with no `regions` member; behaves as a plain leaf |
| [state_with_no_events](12_sm_edge_cases.cpp#L221) | State with no `events` member; all events are unhandled |
| [composite_with_empty_regions](12_sm_edge_cases.cpp#L245) | State with `regions = tlist<>`; triggers `on_regions_finalized` immediately on enter |
| [deep_hierarchy_ten_plus_levels](12_sm_edge_cases.cpp#L264) | 10+ level nested composite; event still dispatches and exits cleanly |
| [many_orthogonal_regions](12_sm_edge_cases.cpp#L293) | Large number of orthogonal regions; all receive event and exit in reverse order |
| [event_not_present_anywhere](12_sm_edge_cases.cpp#L314) | Event not handled at any level; SM silently ignores it |
| [reentrant_event_emission](12_sm_edge_cases.cpp#L331) | State emits `e2` while handling `e1`; `e2` processed after `e1` cycle completes |
| [reentrant_event_emission_chain](12_sm_edge_cases.cpp#L349) | `e1` emits `e2`, `e2` emits `e3`; all three processed in sequence via queue |
| [terminate_stops_region](12_sm_edge_cases.cpp#L369) | State terminates; region is nulled and subsequent events are not dispatched to it |
| [terminate_in_child_stops_only_child](12_sm_edge_cases.cpp#L393) | Child terminates in a multi-region parent; only the child's region is nulled, others continue |
| [static_checks_compile](13_sm_compile_time_diagnostics.cpp#L112) | `static_assert` checks for missing handler, bad return types, and overload legality |
| [orthogonal_two_region_reaction_matrix](14_sm_regression_matrix.cpp#L191) | Matrix of all Forward/Discard/Unhandled/Transit combinations across two orthogonal regions |
| [state_constructs_with_parent_and_context_args](15_sm_state_construction_contexts.cpp#L124) | State constructor receives parent pointer and one state context |
| [state_can_still_default_construct_when_it_expects_nothing](15_sm_state_construction_contexts.cpp#L144) | State with default constructor works even when contexts are present in the SM |
| [state_constructs_with_parent_and_two_contexts](15_sm_state_construction_contexts.cpp#L161) | State constructor receives parent pointer and two state contexts |
| [child_constructor_receives_parent_user_state_type](15_sm_state_construction_contexts.cpp#L181) | Child receives the actual parent user-state type (not the SM wrapper) as parent pointer |
| [triggers_only_when_all_regions_terminated](16_sm_on_regions_complete.cpp#L217) | `on_regions_finalized` fires only after every region has terminated |
| [explicit_target_reaches_nested_state_only](16_sm_on_regions_complete.cpp#L250) | `EvRegionsFinalized` targets the direct owner state; nested ancestors are not notified |
| [on_regions_finalized_can_emit_follow_up_event](16_sm_on_regions_complete.cpp#L268) | `on_regions_finalized` returns `Emit`; follow-up event dispatched in next SM cycle |
| [on_regions_finalized_can_transit_targeted_state](16_sm_on_regions_complete.cpp#L280) | `on_regions_finalized` returns `Transit`; state transitions when all regions are done |
| [on_enter_can_publish_event](17_sm_on_enter.cpp#L52) | `on_enter` returns `Emit`; emitted event flushed at end of construction, before first `post()` |
| [on_enter_noop_does_not_emit](17_sm_on_enter.cpp#L69) | `on_enter` returns `NOOP`; no event enqueued, state lifecycle is normal |
| [parent_enters_before_child](17_sm_on_enter.cpp#L94) | Parent `on_enter` fires before child `on_enter` (construction order guarantee) |
| [invokes_on_exit_on_state_destruction](18_sm_on_exit.cpp#L137) | `on_exit` hook is called when SM goes out of scope |
| [destroys_child_before_parent_on_exit](18_sm_on_exit.cpp#L145) | Child `on_exit` fires before parent `on_exit` during destruction |
| [destroys_regions_in_reverse_order](18_sm_on_exit.cpp#L155) | Orthogonal regions destroyed in reverse declaration order; last-declared exits first |
| [supports_emit_action_on_exit](18_sm_on_exit.cpp#L165) | `on_exit` returning `Emit` compiles and destructs cleanly without throwing |
| [defer_then_transit_flushes_event](19_sm_defer_handling.cpp#L168) | State defers `e_save`; on transit to next state, deferred event is dispatched to new state |
| [deferred_payload_preserved](19_sm_defer_handling.cpp#L187) | Deferred event payload is preserved intact through the defer/flush cycle |
| [multiple_defers_all_flushed](19_sm_defer_handling.cpp#L204) | Multiple deferred events all flushed in order when transit occurs |
| [orthogonal_regions_defer_independently](19_sm_defer_handling.cpp#L228) | Two orthogonal regions each defer different event types independently; both flush on transit |
| [make_intercepted_construction_observer_called](20_sm_coalesce_api.cpp#L157) | Partial API defines only `make`; construction observer fires, on_event falls through to default |
| [capture_discards_without_dispatching_to_region](22_sm_capture_handling.cpp#L8) | `on_capture` returns `Discard`; region's `on_event` never runs |
| [undeclared_capture_falls_through_to_region](22_sm_capture_handling.cpp#L52) | Event not in `captures` list skips `on_capture`; region handles it normally |
| [capture_forward_dispatches_to_region_instead_of_bubbling](22_sm_capture_handling.cpp#L96) | `on_capture` returns `Forward`; propagates down to region instead of bubbling up (opposite of `on_event` `Forward`) |
| [capture_transit_replaces_state_without_region_reacting](22_sm_capture_handling.cpp#L140) | `on_capture` returns `Transit`; whole state replaced directly, region's `on_event` never runs |
| [parent_capture_preempts_nested_child_capture](22_sm_capture_handling.cpp#L190) | Parent's `on_capture` handles the event before the nested child's own `on_capture` is ever checked |
| [on_enter_intercepted_enter_observer_called](20_sm_coalesce_api.cpp#L170) | Partial API defines only `on_enter`; entry observer fires, make and on_event fall through to defaults |
| [on_event_intercepted_event_observer_called](20_sm_coalesce_api.cpp#L183) | Partial API defines only `on_event`; event observer fires per dispatch, all other hooks use defaults |
| [custom_make_spreads_tuple_context_to_state_args](20_sm_coalesce_api.cpp#L202) | Custom API `make` uses `std::apply` to spread a tuple state context into individual state constructor args |
| [type_checks_compile](21_transit_dispatch_state_type_checks.cpp#L133) | Compile-time check of region transit target closure and dispatch target membership |
