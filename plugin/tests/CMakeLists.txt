include(${CMAKE_SOURCE_DIR}/cmake/FetchCatch2.cmake)

add_executable(milkdawp_plugin_tests
  PluginProcessorTests.cpp
)

target_include_directories(milkdawp_plugin_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)

target_link_libraries(milkdawp_plugin_tests PRIVATE
  milkdawp_plugin
  milkdawp_core
  milkdawp_engine
  milkdawp_warnings
  juce::juce_audio_processors
  juce::juce_audio_utils
  Catch2::Catch2WithMain
)
milkdawp_link_external_zlib_libpng(milkdawp_plugin_tests)

include(CTest)
include(Catch)
catch_discover_tests(milkdawp_plugin_tests)
