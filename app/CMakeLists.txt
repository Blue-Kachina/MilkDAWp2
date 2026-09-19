# Real standalone shell work is Phase 4 (D8); until then this skeleton stays
# off by default (MILKDAWP_BUILD_APP=OFF at top level) and Phase 3 uses JUCE's
# Standalone plugin format for an early testable app instead.
juce_add_gui_app(milkdawp_app
  PRODUCT_NAME "MilkDAWp"
  COMPANY_NAME "Otitis Media"
  BUNDLE_ID    com.otitismedia.MilkDAWp
)

target_sources(milkdawp_app PRIVATE src/Main.cpp)
target_compile_features(milkdawp_app PRIVATE cxx_std_20)
target_compile_definitions(milkdawp_app PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)

target_link_libraries(milkdawp_app PRIVATE
  milkdawp_core
  milkdawp_engine
  milkdawp_ui
  milkdawp_warnings
  juce::juce_gui_extra
)
milkdawp_link_external_zlib_libpng(milkdawp_app)

milkdawp_check_single_zlib_libpng(milkdawp_app)
