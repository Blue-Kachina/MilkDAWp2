# cmake/Sanitizers.cmake
#
# Provides `milkdawp_sanitizers`, an INTERFACE target that is a no-op unless
# one of the options below is turned on. Link it into every core/engine
# target and their test executables (§8 testing strategy):
#   - MILKDAWP_SANITIZE_ADDRESS_UB: ASan + UBSan, for core/engine tests.
#   - MILKDAWP_SANITIZE_THREAD:     TSan, for the AudioRing/Messages queue
#                                   tests (Phase 1.1/1.2) once they exist.
#   - MILKDAWP_SANITIZE_REALTIME:   Clang's RealtimeSanitizer, for functions
#                                   marked [[clang::nonblocking]] -- the audio
#                                   callback path (Phase 3.1's processBlock).
#                                   Clang-only; GCC has no equivalent.
#
# ASan/UBSan and TSan cannot be combined in one binary; RTSan is independent
# of both. Each gets its own CI job and CMake preset (ci-linux-{asan,tsan,rtsan}).
option(MILKDAWP_SANITIZE_ADDRESS_UB "Build core/engine with ASan+UBSan" OFF)
option(MILKDAWP_SANITIZE_THREAD "Build core/engine with ThreadSanitizer" OFF)
option(MILKDAWP_SANITIZE_REALTIME "Build with Clang's RealtimeSanitizer (Clang only)" OFF)

if(MILKDAWP_SANITIZE_ADDRESS_UB AND MILKDAWP_SANITIZE_THREAD)
  message(FATAL_ERROR "MILKDAWP_SANITIZE_ADDRESS_UB and MILKDAWP_SANITIZE_THREAD are mutually exclusive in one build")
endif()

add_library(milkdawp_sanitizers INTERFACE)

if(MILKDAWP_SANITIZE_ADDRESS_UB)
  target_compile_options(milkdawp_sanitizers INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
  target_link_options(milkdawp_sanitizers INTERFACE -fsanitize=address,undefined)
endif()

if(MILKDAWP_SANITIZE_THREAD)
  target_compile_options(milkdawp_sanitizers INTERFACE -fsanitize=thread -fno-omit-frame-pointer -g)
  target_link_options(milkdawp_sanitizers INTERFACE -fsanitize=thread)
endif()

if(MILKDAWP_SANITIZE_REALTIME)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "MILKDAWP_SANITIZE_REALTIME requires Clang (got ${CMAKE_CXX_COMPILER_ID})")
  endif()
  target_compile_options(milkdawp_sanitizers INTERFACE -fsanitize=realtime -fno-omit-frame-pointer -g)
  target_link_options(milkdawp_sanitizers INTERFACE -fsanitize=realtime)
endif()
