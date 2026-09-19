# cmake/ProjectMDependency.cmake
#
# Locates libprojectM (vcpkg-provided; see vcpkg.json) and validates it is a
# SHARED library, which projectM's LGPL-2.1 licence requires when dynamically
# linked into MilkDAWp's AGPL-3.0-or-later binary (D10). The triplets/*.cmake
# overlay triplets force this; this check catches a misconfigured triplet.
#
# Exposes MILKDAWP_PROJECTM_TARGET for consumers once milkdawp_engine exists.

if(NOT MILKDAWP_WITH_PROJECTM)
  return()
endif()

find_package(projectM4 CONFIG QUIET)
if(NOT projectM4_FOUND)
  find_package(projectm CONFIG QUIET)
endif()
if(NOT projectM4_FOUND AND NOT projectm_FOUND)
  message(FATAL_ERROR
    "MILKDAWP_WITH_PROJECTM=ON but neither the projectM4 nor projectm vcpkg CMake "
    "package was found. Check that vcpkg installed 'projectm' for triplet "
    "${VCPKG_TARGET_TRIPLET}, and that VCPKG_OVERLAY_TRIPLETS points at triplets/.")
endif()

set(MILKDAWP_PROJECTM_TARGET "")
foreach(_mdw_candidate IN ITEMS libprojectM::projectM projectM::projectM projectm::projectm projectM projectm)
  if(TARGET ${_mdw_candidate})
    set(MILKDAWP_PROJECTM_TARGET ${_mdw_candidate})
    break()
  endif()
endforeach()
unset(_mdw_candidate)

if(NOT MILKDAWP_PROJECTM_TARGET)
  message(FATAL_ERROR
    "libprojectM was found but exposes none of the expected CMake targets "
    "(libprojectM::projectM, projectM::projectM, projectm::projectm, projectM, projectm).")
endif()

get_target_property(_mdw_projectm_type ${MILKDAWP_PROJECTM_TARGET} TYPE)
if(NOT _mdw_projectm_type STREQUAL "SHARED_LIBRARY" AND NOT _mdw_projectm_type STREQUAL "UNKNOWN_LIBRARY")
  message(WARNING
    "libprojectM target '${MILKDAWP_PROJECTM_TARGET}' is ${_mdw_projectm_type}, not "
    "SHARED_LIBRARY. Confirm VCPKG_TARGET_TRIPLET is one of the *-dynamic triplets in triplets/.")
endif()
unset(_mdw_projectm_type)

message(STATUS "MilkDAWp: using projectM target ${MILKDAWP_PROJECTM_TARGET}")
