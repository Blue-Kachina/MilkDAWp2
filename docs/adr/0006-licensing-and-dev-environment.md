# ADR-0006: Licensing and development environment

Status: Decided

## Context

JUCE 9 is dual-licensed (AGPLv3 or a commercial EULA); projectM is
LGPL-2.1 and must be dynamically linked to avoid triggering its copyleft on
the whole binary. Separately, cross-platform plugin development cannot be
fully containerized -- macOS binaries need Apple's toolchain on macOS,
DAW-grade Windows binaries need MSVC, and containers have no GPU or audio
devices -- but everything that doesn't need those (the JUCE-free core,
`mdw-analyze`, headless render tests, lint, docs) can be, and that's where
most iteration happens.

## Decision

- **Licensing (D10):** MilkDAWp 2 stays AGPL-3.0-or-later (the JUCE 9 AGPLv3
  path). projectM (LGPL-2.1) is linked dynamically via the `*-dynamic` vcpkg
  triplets, validated at configure time (`cmake/ProjectMDependency.cmake`).
  Moving off AGPL would require the commercial JUCE 9 licence -- that's a
  business decision, not a code change, and needs a new ADR if it ever comes
  up.
- **Development environment (D14):** one container image
  (`.devcontainer/Dockerfile`) covering the core, CLI, headless render, and
  lint, used identically by CI (`container:`), local devcontainers, and
  Claude Code web sessions. CI is the Windows/macOS build farm (native
  runners, no containerization needed or possible there). Idempotent native
  bootstrap scripts (`scripts/bootstrap.{sh,ps1}`) with a `--doctor` mode
  exist for contributors who want to build natively instead. No Nix: it
  would help pin toolchains on macOS/Linux but doesn't cover Windows and adds
  a learning curve for a project this size.

## Consequences

- Every MilkDAWp binary must dynamically link projectM; a static-projectM
  build option is out of scope (v1's `MILKDAWP_PROJECTM_LINK_STATIC` had this
  and it's intentionally not carried forward).
- The devcontainer image is rebuilt and republished whenever the Dockerfile,
  vcpkg manifests, or the JUCE pin changes, and CI runs *inside* the
  published image so "works in the container" and "works in CI" can't drift
  apart silently.
