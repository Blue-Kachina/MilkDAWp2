# Plugin identity is decided (D1): keep v1's manufacturer/plugin codes and
# bundle ID so existing hosts/sessions keep resolving MilkDAWp. Do not change
# these without an ADR (see the working agreements in development_roadmap.md §11).
juce_add_plugin(milkdawp_plugin
  COMPANY_NAME                "Otitis Media"
  BUNDLE_ID                   com.otitismedia.MilkDAWp
  IS_SYNTH                    FALSE
  NEEDS_MIDI_INPUT            FALSE
  NEEDS_MIDI_OUTPUT           FALSE
  IS_MIDI_EFFECT              FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  PLUGIN_MANUFACTURER_CODE    OMda
  PLUGIN_CODE                 Mlkw
  FORMATS                     VST3
  VST3_CATEGORIES             "Fx" "Analyzer"
  PRODUCT_NAME                "MilkDAWp"
)

target_sources(milkdawp_plugin PRIVATE
  src/PluginProcessor.h
  src/PluginProcessor.cpp
  src/PluginEditor.h
  src/PluginEditor.cpp
)

target_compile_features(milkdawp_plugin PRIVATE cxx_std_20)
target_compile_definitions(milkdawp_plugin PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(milkdawp_plugin PRIVATE
  milkdawp_core
  milkdawp_engine
  milkdawp_ui
  milkdawp_warnings
  juce::juce_audio_utils
  juce::juce_audio_processors
  juce::juce_gui_basics
  juce::juce_gui_extra
  juce::juce_dsp
  juce::juce_opengl
)
milkdawp_link_external_zlib_libpng(milkdawp_plugin)

milkdawp_check_single_zlib_libpng(milkdawp_plugin_VST3)
