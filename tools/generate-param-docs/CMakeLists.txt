# Regenerates docs/parameters.md from ParameterModel (Phase 1.13). Run and
# redirect after any change to core/src/ParameterModel.cpp:
#   cmake --build <builddir> --target generate-param-docs
#   ./generate-param-docs > docs/parameters.md
add_executable(generate-param-docs src/main.cpp)
target_compile_features(generate-param-docs PRIVATE cxx_std_20)
target_link_libraries(generate-param-docs PRIVATE milkdawp_core milkdawp_warnings)
