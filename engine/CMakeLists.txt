add_library(milkdawp_engine STATIC
  include/milkdawp/engine/EngineInfo.h
  src/EngineInfo.cpp
  include/milkdawp/engine/ProjectMLibrary.h
  src/ProjectMLibrary.cpp
)

target_include_directories(milkdawp_engine PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(milkdawp_engine PUBLIC cxx_std_20)
target_link_libraries(milkdawp_engine
  PUBLIC
    milkdawp_core
    milkdawp_sanitizers
    juce::juce_core # ProjectMLibrary.h is a public header exposing juce::File/DynamicLibrary
  PRIVATE
    milkdawp_warnings
    juce::juce_opengl
)
milkdawp_link_external_zlib_libpng(milkdawp_engine)

# projectM linkage lands in Phase 2 (RenderEngine, ProjectMLibrary); the
# skeleton target only records whether it was available at configure time.
if(MILKDAWP_WITH_PROJECTM AND MILKDAWP_PROJECTM_TARGET AND TARGET ${MILKDAWP_PROJECTM_TARGET})
  target_link_libraries(milkdawp_engine PRIVATE ${MILKDAWP_PROJECTM_TARGET})
  target_compile_definitions(milkdawp_engine PRIVATE MILKDAWP_HAS_PROJECTM=1)
else()
  target_compile_definitions(milkdawp_engine PRIVATE MILKDAWP_HAS_PROJECTM=0)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
