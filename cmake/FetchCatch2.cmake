# cmake/FetchCatch2.cmake
#
# Vendors Catch2 v3 (D6) via FetchContent, pinned to a release tag and its
# commit hash, same pattern as cmake/FetchJuce.cmake. milkdawp_core cannot use
# juce::UnitTest since it has no JUCE dependency by design (§4.1).

include(FetchContent)

set(MILKDAWP_CATCH2_TAG "v3.9.1" CACHE STRING "Catch2 release tag to vendor")
set(MILKDAWP_CATCH2_COMMIT "dfc2dff8d70d083c60c1c6986030e5389a867a93" CACHE STRING
    "Commit hash that MILKDAWP_CATCH2_TAG must resolve to")

FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        ${MILKDAWP_CATCH2_COMMIT}
  GIT_SHALLOW    TRUE
  SYSTEM
)
FetchContent_MakeAvailable(catch2)

list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
