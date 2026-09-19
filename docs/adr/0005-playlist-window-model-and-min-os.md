# ADR-0005: Playlist ownership, window model, and minimum OS

Status: Decided (D13) / Recommended (D7, D9)

## Context

`libprojectM-4-playlist` exists, but MilkDAWp needs ratings, tags, a
no-repeat history window, and sample-accurate scheduling (§4.4) that it
doesn't offer. Separately, v1's editor was a control strip with the video
popping out into its own window; every pop-out reparented the GL component,
which JUCE turns into a GL-context recreation, which resets projectM (§2.4)
-- a direct consequence of the old window model, not a bug to patch in
isolation.

## Decision

- **Playlist (D7):** own `Playlist` implementation in `milkdawp_core`
  (folder scan, shuffle-no-repeat, lock, weighted policies -- Phase 1.11),
  not `libprojectM-4-playlist`.
- **Window model (D13):** video-first. In both shells the primary window
  *is* the visualization; controls live in a hover/tap/pinned `ControlDrawer`
  over it. A separately owned `OutputWindow` handles fullscreen on a second
  display; a detached-controls window is a secondary, low-cost feature since
  it reuses the same drawer component (§4.9).
- **Minimum OS (D9):** Windows 10 21H2+, macOS 12+ (universal x86_64 +
  arm64), Ubuntu 22.04+ / glibc 2.35+. Standalone loopback capture needs
  macOS 13+/14.2+ and is feature-gated at runtime, not a floor-raise.

## Consequences

- The render engine (Phase 2) owns projectM for the processor/app's
  lifetime; windows attach and detach from it as pure presentation surfaces,
  so no window operation can reset the visual (§4.5 fixes §2.4).
- `triplets/*.cmake` set `VCPKG_OSX_DEPLOYMENT_TARGET "12.0"` to match D9
  (v1 used 11.0); any macOS-version-gated feature (loopback capture) checks
  at runtime, not at compile time.
