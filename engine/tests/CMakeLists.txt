include(${CMAKE_SOURCE_DIR}/cmake/FetchCatch2.cmake)

add_executable(milkdawp_engine_tests
  ProjectMLibraryTests.cpp
)

target_link_libraries(milkdawp_engine_tests PRIVATE
  milkdawp_engine
  milkdawp_warnings
  milkdawp_sanitizers
  Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(milkdawp_engine_tests)
