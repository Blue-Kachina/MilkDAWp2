#!/bin/sh
# Claude Code web session-start hook (Phase 0.11, D14).
#
# Goal: a fresh Claude Code web session can build and run milkdawp_core's
# tests immediately, without an agent spending its first turn discovering
# that vcpkg/JUCE/projectM need to be fetched and built from scratch.
#
# This script is a no-op everywhere except a session actually running inside
# (or reusing) the devcontainer image from .devcontainer/Dockerfile, since
# that's the only place VCPKG_ROOT/FETCHCONTENT_SOURCE_DIR_JUCE point at
# pre-built content. On a plain checkout it just gets out of the way.

set -eu

cd "$(dirname "$0")/../.." || exit 1

if [ -z "${VCPKG_ROOT:-}" ] || [ ! -x "${VCPKG_ROOT}/vcpkg" ]; then
  echo "session-start: VCPKG_ROOT not set to a built vcpkg (not running in the devcontainer image?); skipping warm-up." >&2
  exit 0
fi

echo "session-start: configuring dev-linux preset..."
cmake --preset dev-linux -DMILKDAWP_WITH_PROJECTM=ON >/dev/null

echo "session-start: building milkdawp_core_tests and mdw-analyze (Debug)..."
cmake --build --preset dev-linux-Debug --target milkdawp_core_tests mdw-analyze >/dev/null

echo "session-start: running milkdawp_core tests to confirm the build is warm..."
ctest --test-dir build-linux -C Debug --output-on-failure

echo "session-start: ready. Try: ctest --test-dir build-linux -C Debug"
