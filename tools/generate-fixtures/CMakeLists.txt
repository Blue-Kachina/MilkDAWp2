# Generates the synthesized fixture set under fixtures/ (Phase 1.9). Run:
#   cmake --build <builddir> --target generate-fixtures
#   ./generate-fixtures <repoRoot>/fixtures
add_executable(generate-fixtures src/main.cpp)
target_compile_features(generate-fixtures PRIVATE cxx_std_20)
target_link_libraries(generate-fixtures PRIVATE milkdawp_core milkdawp_warnings)
