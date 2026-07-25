# Agent Instructions

## Build Commands

| Step | Command |
|------|---------|
| Configure (tests + sandbox) | `./configure/gcc -ts` or `./configure/clang -ts` |
| Build | `ninja -C .build` |
| Run tests | `.build/bin/sm_test` |
| Run sandbox | `.build/bin/sandbox` |

Flags for configure: `-t` = tests, `-s` = sandbox. Build output goes to `.build/`, executables to `.build/bin/`.

## Workflow Rules

- **Always consult the user before modifying any file or applying any fix.**
- Work one step at a time. Show the proposed change, wait for confirmation, then apply.
- After each change, build and run the relevant tests to verify before moving on.
- When adding tests, follow the existing pattern in that file (local `struct`s inside `TEST` bodies, `TestSM` + `APIMock` for lifecycle tests).

## Test Overview

See [`src/test/TESTS.md`](src/test/TESTS.md) for a full table of all tests with descriptions and links.

## Test Conventions

### Structure
- Define state types as **local `struct`s inside the `TEST` body** — not named classes at file scope. This prevents name collisions between tests that reuse names like `Child` or `Parent`.
- Use `TestSM<TopState>` with `std::make_tuple()` (state contexts) and `&mock` (API contexts).
- Use `testing::StrictMock<APIMock>` with `testing::InSequence` to enforce call order.

### Expected call sequence
Lifecycle calls follow a strict order determined by construction/destruction:
1. **Enter phase** (construction order): `on_make` → `on_enter` for each state, parent before child, regions in declaration order.
2. **Event phase**: `on_event` calls follow dispatch order — deepest child first, bubbling up on `Forward`.
3. **Exit phase** (destruction order): deepest child first, regions in reverse declaration order, parent last.

### Inline comments
Add a brief rationale comment above the key `EXPECT_CALL` groups using `→` notation:
```cpp
// Child forwards → Parent handles and discards (stops bubbling)
EXPECT_CALL(mock, on_event_called(type_id<Child>, type_id<e1>)).Times(1);
EXPECT_CALL(mock, on_event_called(type_id<Parent>, type_id<e1>)).Times(1);
```

### EXPECT_CALL placement
Place `EXPECT_CALL` immediately before the `post` (or other call) that triggers it — not at the top of the test. This makes the causal relationship explicit:
```cpp
sm.post(e_save{});   // defers — no expectation here

EXPECT_CALL(obs, event_received()).Times(1);
sm.post(e_go{});     // triggers the flush → observer call
```

### AAA Pattern (Arrange-Act-Assert)
Tests follow the **AAA pattern** with these rules:
- **Arrange**: Set up state and **expectations before the trigger**. `EXPECT_CALL` must be placed immediately before its trigger.
- **Act**: Execute exactly one trigger per AAA section (e.g., `post`, SM destruction)
- **Assert**: Check results using explicit assertions (e.g., `ASSERT_EQ`, `ASSERT_TRUE`), or rely on implicit assertions via `EXPECT_CALL` verification
- Tests can have **multiple AAA sections** (multiple triggers in sequence)
- **One trigger per section** — all `EXPECT_CALL` in a section must be for that section's trigger
- No global expectations at the top of the test; expectations live immediately before their trigger
- **Each AAA section is enclosed in its own `{}` block.** If there is only one section in the whole test, no block is needed.
- **`testing::InSequence` lives at test scope** (declared at the top of the test body, not inside a block)
- **SM construction lives at test scope** — the `TestSM sm(...)` variable is declared between section blocks, not inside any block
- **Construction and destruction sections do not need a `{}` block** — their triggers (SM declaration and implicit end-of-test destruction) are already at test scope, so `EXPECT_CALL`s for those sections live at test scope too. Only `post` sections require a `{}` block.

Example with three AAA sections:
```cpp
TEST(suite, name)
{
    testing::StrictMock<APIMock> mock;
    testing::InSequence seq;

    // Section 1: Construction (no block — SM is declared at test scope)
    EXPECT_CALL(mock, on_make_called(...)).Times(1);
    EXPECT_CALL(mock, on_enter_called(...)).Times(1);
    TestSM<...> sm(std::make_tuple(), &mock);

    // Section 2: Event processing (block — post is self-contained)
    {
        EXPECT_CALL(mock, on_event_called(...)).Times(1);
        sm.post(e1{});
    }

    // Section 3: Destruction (no block — implicit at end of test)
    EXPECT_CALL(mock, on_exit_called(...)).Times(1);
}
```

### Test naming
- Use `snake_case` test names that describe the **scenario and outcome**: `parent_skips_discarded_child_event`, `child_transit_does_not_bubble_to_parent`.
- Avoid names that only describe the setup without the expected behavior.

### When `Forward` needs to be meaningful
If a state returns `Forward` and is the **top-level** state in the SM, the forward is unobservable (equivalent to `Discard`). To test that `Forward` actually bubbles, add a grandparent layer that handles the event.

### Emit/observer tests
For tests that verify emitted event payloads, define a custom SM type with the observer passed as a state context (see `09_sm_emit_handling.cpp` for the pattern). Do not use `APIMock` for these — it does not track emitted event delivery.
