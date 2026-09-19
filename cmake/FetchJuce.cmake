# cmake/FetchJuce.cmake
#
# Vendors JUCE 9 via FetchContent, pinned to a release tag AND its commit hash
# (D4 / §4.11 of development_roadmap.md): the vcpkg `juce` port still lags at
# 8.0.7, and JUCE's own CMake API is designed for add_subdirectory/FetchContent
# rather than a package manager.
#
# To upgrade JUCE: bump both MILKDAWP_JUCE_TAG and MILKDAWP_JUCE_COMMIT on a
# branch, run the full CI matrix and the DAW checklist, and review
# JUCE's BREAKING_CHANGES.md before merging (see §10 risks).
#
# The devcontainer image and CI pre-fetch this source (Phase 0.9/0.10) by
# setting FETCHCONTENT_SOURCE_DIR_JUCE, which FetchContent honours
# automatically -- no code here needs to change for that to work.

include(FetchContent)

set(MILKDAWP_JUCE_TAG "9.0.2" CACHE STRING "JUCE release tag to vendor")
set(MILKDAWP_JUCE_COMMIT "72782788ce18c2d4d760b28e0921d6ffc6431102" CACHE STRING
    "Commit hash that MILKDAWP_JUCE_TAG must resolve to; pinning both catches a moved/re-tagged release")

FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG        ${MILKDAWP_JUCE_COMMIT}
  GIT_SHALLOW    TRUE
  SYSTEM
)
FetchContent_MakeAvailable(juce)
