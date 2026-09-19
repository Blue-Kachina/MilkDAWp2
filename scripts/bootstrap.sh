#!/bin/sh
# Idempotent native bootstrap for MilkDAWp 2 on macOS (Phase 0.12, D14).
# Installs (via Homebrew) the minimum toolchain pinned in toolchain.json:
# Xcode Command Line Tools, CMake, and Ninja. Safe to re-run.
#
# Usage:
#   scripts/bootstrap.sh            install anything missing
#   scripts/bootstrap.sh --doctor   report found vs. required, install nothing,
#                                    exit non-zero if anything is missing

set -eu

REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)
TOOLCHAIN_JSON="$REPO_ROOT/toolchain.json"

doctor=0
for arg in "$@"; do
  case "$arg" in
    --doctor) doctor=1 ;;
  esac
done

# toolchain.json is small and controlled by us; python3 is present on every
# Mac (it's the same Xcode CLT stub this script installs below), so use it
# rather than hand-rolling a JSON parser.
tc_get() {
  python3 -c "
import json, sys
data = json.load(open('$TOOLCHAIN_JSON'))
for part in '$1'.split('.'):
    data = data[part]
print(data)
"
}

version_ge() {
  # version_ge <found> <required> -- true if found >= required
  [ "$1" = "$2" ] && return 0
  highest=$(printf '%s\n%s\n' "$1" "$2" | sort -V | tail -n1)
  [ "$highest" = "$1" ]
}

ok=1

echo "Tool                 Required   Found        Status"
echo "-------------------- ---------- ------------ -------"

check() {
  name=$1
  required=$2
  found=$3
  if [ -z "$found" ]; then
    status="MISSING"
    ok=0
  elif version_ge "$found" "$required"; then
    status="ok"
  else
    status="TOO OLD"
    ok=0
  fi
  printf "%-20s %-10s %-12s %s\n" "$name" "$required" "${found:-none}" "$status"
}

cmake_required=$(tc_get cmake.minVersion)
cmake_found=$(command -v cmake >/dev/null 2>&1 && cmake --version | head -n1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || true)
check "cmake" "$cmake_required" "$cmake_found"

ninja_required=$(tc_get ninja.minVersion)
ninja_found=$(command -v ninja >/dev/null 2>&1 && ninja --version || true)
check "ninja" "$ninja_required" "$ninja_found"

clt_required=$(tc_get macos.xcodeCommandLineTools.minVersion)
clt_found=$(xcode-select -p >/dev/null 2>&1 && pkgutil --pkg-info=com.apple.pkg.CLTools_Executables 2>/dev/null | awk '/^version:/ {print $2}' || true)
check "Xcode CLT" "$clt_required" "$clt_found"

if [ "$doctor" -eq 1 ]; then
  [ "$ok" -eq 1 ] && exit 0 || exit 1
fi

if [ "$ok" -eq 1 ]; then
  echo "All required tools already meet the minimum version. Nothing to install."
  exit 0
fi

if ! xcode-select -p >/dev/null 2>&1; then
  echo "Installing Xcode Command Line Tools (this opens a GUI installer)..."
  xcode-select --install
  echo "Re-run this script once the Xcode CLT install finishes."
  exit 1
fi

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew not found. Install it from https://brew.sh, then re-run this script." >&2
  exit 1
fi

command -v cmake >/dev/null 2>&1 || brew install cmake
command -v ninja >/dev/null 2>&1 || brew install ninja

echo "Bootstrap complete. Re-run with --doctor to verify."
