add_library(milkdawp_engine STATIC
  include/milkdawp/engine/EngineInfo.h
  src/EngineInfo.cpp
  include/milkdawp/engine/ProjectMLibrary.h
  src/ProjectMLibrary.cpp
  include/milkdawp/engine/RenderEngine.h
  src/RenderEngine.cpp
  include/milkdawp/engine/PresetLoader.h
  src/PresetLoader.cpp
  include/milkdawp/engine/TransitionExecutor.h
  src/TransitionExecutor.cpp
  include/milkdawp/engine/OutputSurface.h
  src/OutputSurface.cpp
)

target_include_directories(milkdawp_engine PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(milkdawp_engine PUBLIC cxx_std_20)
target_link_libraries(milkdawp_engine
  PUBLIC
    milkdawp_core
    milkdawp_sanitizers
    juce::juce_core   # ProjectMLibrary.h is a public header exposing juce::File/DynamicLibrary
    juce::juce_opengl # RenderEngine.h/OutputSurface.h are public headers exposing juce::OpenGLContext
  PRIVATE
    milkdawp_warnings
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
