add_library(milkdawp_ui STATIC
  include/milkdawp/ui/UiInfo.h
  src/UiInfo.cpp
)

target_include_directories(milkdawp_ui PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_features(milkdawp_ui PUBLIC cxx_std_20)
target_compile_definitions(milkdawp_ui PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)
target_link_libraries(milkdawp_ui
  PUBLIC
    milkdawp_core
  PRIVATE
    milkdawp_warnings
    juce::juce_gui_basics
    juce::juce_gui_extra
)
milkdawp_link_external_zlib_libpng(milkdawp_ui)
