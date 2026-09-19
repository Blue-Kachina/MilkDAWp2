# ADR-0004: Language, toolchain, and test framework

Status: Recommended

## Context

`milkdawp_core` has no JUCE dependency by design (§4.1), so it cannot use
`juce::UnitTest`. The project also needs warnings-as-errors and a consistent
minimum toolchain across three platforms to keep "works on my machine" from
becoming a recurring CI failure mode.

## Decision

- **Language and toolchain (D5):** C++20. MSVC 2022, Apple Clang 15+,
  GCC 12+ / Clang 16+. Warnings as errors on our own targets only
  (`milkdawp_warnings` in `cmake/Warnings.cmake`), never on vendored JUCE,
  Catch2, or projectM. `clang-format` + `clang-tidy` configs are committed
  (`.clang-format`, `.clang-tidy`) and enforced by a pre-commit hook
  (`scripts/hooks/pre-commit`).
- **Test framework (D6):** Catch2 v3, vendored via `FetchContent` pinned to a
  tag and commit hash (`cmake/FetchCatch2.cmake`), for `milkdawp_core` and
  `milkdawp_engine` tests. `pluginval` validates the plugin binaries
  themselves (Phase 3.9); it is not a substitute for unit tests.

## Consequences

- Core/engine code cannot depend on any JUCE type reaching a test, which
  keeps those tests running everywhere Catch2 runs, including CI containers
  with no display or audio device.
- A third C++ test framework (e.g. GoogleTest) showing up anywhere in the
  tree is a "delete one first" situation per the working agreements (§11),
  not something to add alongside Catch2.
