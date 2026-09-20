# ADR-0007: Dev-only alternate plugin identity for side-by-side testing

Status: Accepted

## Context

ADR-0001 (D1) deliberately has v2 reuse v1's exact identity: manufacturer
code `OMda`, plugin code `Mlkw`, bundle ID `com.otitismedia.MilkDAWp`,
product name `MilkDAWp`. That ADR's own "Consequences" section already
flagged the cost: "never install a v1 and v2 build into the same plugin
folder at once... a host will pick one arbitrarily."

In practice the collision is worse than "same folder": REAPER (and VST3
hosts generally) key their plugin database by the VST3 class ID, which
JUCE derives from the manufacturer+plugin code pair, not by filesystem
path. Matthew has a real, working v1 `MilkDAWp.vst3` installed (used day to
day) and wants to keep it while test-driving v2 builds in the same REAPER
install. With identical codes, REAPER's plugin database only ever keeps one
entry for `MilkDAWp.vst3`, regardless of which directories are on the VST3
scan path -- exactly this ambiguity, hit in practice during Phase 2.3's
first real-host test (2026-09-19).

## Decision

Add a CMake option, `MILKDAWP_DEV_ALT_IDENTITY` (default `OFF`), to
`plugin/CMakeLists.txt`. When `ON`:

- `PLUGIN_CODE` becomes `Mlk2` instead of `Mlkw`.
- `PRODUCT_NAME` becomes `MilkDAWp2 Dev` instead of `MilkDAWp`.
- `BUNDLE_ID` becomes `com.otitismedia.MilkDAWp2Dev` instead of
  `com.otitismedia.MilkDAWp`.
- `PLUGIN_MANUFACTURER_CODE` (`OMda`) is unchanged either way -- Otitis
  Media still made it, and the manufacturer code alone isn't what collides.

This is **exclusively a local development convenience**. It is never used
for a release build (`release-*` presets don't set it, and it must stay
opt-in). Default builds continue to match ADR-0001 exactly.

## Consequences

- A developer who wants a v1 install and a v2 dev build loadable side by
  side in the same host configures with `-DMILKDAWP_DEV_ALT_IDENTITY=ON`;
  the result shows up as a distinctly named, distinctly identified plugin
  ("MilkDAWp2 Dev") that cannot collide with a real v1 or v2 install.
- Any state saved under the alternate identity is **not** portable to the
  real `MilkDAWp` identity (different VST3 class ID entirely) and is not
  meant to be -- it is throwaway dev-session state, not something Phase
  3.2's state migration needs to care about.
- This does not change ADR-0001's decision. The real 1.0 release, and every
  default/CI/release-preset build, still ships as `MilkDAWp` with v1's exact
  codes, unchanged.
