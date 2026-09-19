# JUCE-free by design (§4.1): mdw-analyze links only milkdawp_core so beat
# detection and transition logic can be iterated on without a display or GPU.
add_executable(mdw-analyze src/main.cpp src/RunSuite.cpp)

target_compile_features(mdw-analyze PRIVATE cxx_std_20)
target_link_libraries(mdw-analyze PRIVATE milkdawp_core milkdawp_warnings)

milkdawp_check_single_zlib_libpng(mdw-analyze)
