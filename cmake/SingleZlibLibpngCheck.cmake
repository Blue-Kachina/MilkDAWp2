# cmake/SingleZlibLibpngCheck.cmake
#
# JUCE 9 compiles its bundled zlib/libpng as C, not wrapped in a C++ namespace
# (§4.11 of development_roadmap.md), which risks an ODR violation if vcpkg's
# zlib/libpng -- pulled in transitively by projectM and freetype -- also link
# into the same binary.
#
# Decision (D4/§4.11): when projectM is linked in (it and freetype pull in
# vcpkg's zlib/libpng), use vcpkg's copies everywhere and disable JUCE's
# bundled ones, so only one copy of each can ever link into a MilkDAWp binary.
# When projectM isn't linked (e.g. a core/engine-skeleton build with no vcpkg
# available, as in Phase 0), there is no second copy to conflict with, so
# JUCE's own bundled zlib/libpng are left enabled -- forcing them off without
# an external zlib/libpng target to link would just fail the build.
option(MILKDAWP_JUCE_ZLIB_LIBPNG_FROM_VCPKG
  "Disable JUCE's bundled zlib/libpng and rely on vcpkg's copies instead (avoids ODR conflicts, §4.11)"
  ${MILKDAWP_WITH_PROJECTM})

if(MILKDAWP_JUCE_ZLIB_LIBPNG_FROM_VCPKG)
  add_compile_definitions(JUCE_INCLUDE_ZLIB_CODE=0 JUCE_INCLUDE_PNGLIB_CODE=0)
  find_package(ZLIB REQUIRED)
  find_package(PNG REQUIRED)
endif()

# milkdawp_link_external_zlib_libpng(<target>)
#
# Links vcpkg's zlib/libpng into <target> when MILKDAWP_JUCE_ZLIB_LIBPNG_FROM_VCPKG
# is ON (required once JUCE's bundled copies are disabled above); a no-op
# otherwise. Call this on every target that links a JUCE module using zlib or
# libpng (juce_core, juce_graphics) -- i.e. milkdawp_engine, milkdawp_ui,
# milkdawp_plugin, milkdawp_app.
function(milkdawp_link_external_zlib_libpng target)
  if(MILKDAWP_JUCE_ZLIB_LIBPNG_FROM_VCPKG)
    target_link_libraries(${target} PRIVATE ZLIB::ZLIB PNG::PNG)
  endif()
endfunction()

# milkdawp_check_single_zlib_libpng(<target>)
#
# Adds a POST_BUILD step enforcing the decision above at link time: it lists
# <target>'s shared-library dependencies and fails the build if more than one
# zlib or more than one libpng copy would load at runtime.
#
# Call this on every final linked binary (plugin, app, mdw-analyze, engine
# tests) once those targets exist -- there is nothing to check yet in Phase 0.
function(milkdawp_check_single_zlib_libpng target)
  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
      "-DMILKDAWP_CHECK_TARGET_FILE=$<TARGET_FILE:${target}>"
      "-DMILKDAWP_CHECK_PLATFORM=${CMAKE_SYSTEM_NAME}"
      -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/scripts/check_single_zlib_libpng.cmake"
    VERBATIM
    COMMENT "Checking ${target} links exactly one zlib and one libpng copy"
  )
endfunction()
