# Agent Instructions

## Project Commands

| Step | Command |
|------|---------|
| Configure with tests and auxiliary tools | `./configure/gcc -ts` or `./configure/clang -ts` |
| Build | `ninja -C .build` |
| Run tests | `.build/bin/sm_test` |
| Run auxiliary tool | `.build/bin/sandbox` |

Configure flags: `-t` enables tests and `-s` enables auxiliary tools. Build output goes to `.build/`, with executables in `.build/bin/`.

## Workflow Rules

- **Always consult the user before modifying any file or applying any fix.**
- Work incrementally. State the proposed change, wait for confirmation, then apply it.
- Keep changes focused and preserve unrelated user changes.
- After each change, run the narrowest relevant validation available before moving on.
- Follow existing code and test patterns in the files being changed.
- Add focused regression coverage when behavior changes or a bug fix lacks a test.

## Header Include Order

- Order includes as local project headers, third-party headers, then STL headers.
- Use relative includes for headers within the same project subdirectory; keep public installed include paths unchanged.

## Testing Guidance

- Structure tests using Arrange, Act, Assert.
- Keep exactly one meaningful trigger or action per AAA block.
- Put setup and expectations immediately before the trigger they govern.
- Put assertions immediately after the action, or rely on implicit framework verification when appropriate.
- Use explicit `{}` blocks to scope setup, expectations, temporary fixtures, and assertions when a test contains multiple independent AAA sections.
- A helper method that fully owns one isolated AAA sequence does not need an additional `{}` block at every call site.
- Construction and destruction sections do not need a `{}` block when the subject under test is created and destroyed at test scope; action-trigger sections require a separate block.
- Put independent actions in separate blocks, each with its own arrangement and verification.
- Keep shared fixtures at test scope only when their lifetime must span multiple blocks.
- Keep lifecycle-sensitive setup or teardown at the scope where the test intentionally observes it.
- Avoid global expectations or mutable setup that silently affects unrelated blocks.
- Use mocks for test dependencies; do not use stubs or hand-written fakes.
- Always use strict mocks, such as `StrictMock`, so unexpected interactions fail the test.
- Control mocked dependency behavior only through explicit expectations; use `EXPECT_CALL` for every expected interaction.
- Treat interaction order as part of the contract and always use `InSequence` for mock expectations. `InSequence` may be declared at test scope when it governs the whole test, or inside a narrower block when it governs only that block.
- Use comments only to explain non-obvious causal relationships.
- Use descriptive test names that state the scenario and expected outcome.
- Prefer focused regression tests for changed behavior, followed by the broader suite when practical.
- Do not change unrelated tests or weaken existing assertions to make a build pass.
