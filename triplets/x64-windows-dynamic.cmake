# MilkDAWp 2 custom triplet: x64 Windows, dynamic linking.
# Forces every vcpkg dependency (notably projectM) to build as a shared library
# (DLL), which projectM's LGPL-2.1 licence requires for our AGPL-linked binary.
# Pairs with CMAKE_MSVC_RUNTIME_LIBRARY=...DLL in the top-level CMakeLists.

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

# Use the default Windows toolchain.
set(VCPKG_CMAKE_SYSTEM_NAME "")

# Workaround for ports with legacy CMakeLists that fail under CMake 4.x policy
# defaults (e.g. bzip2). Applied to all port configure invocations.
set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DCMAKE_POLICY_VERSION_MINIMUM=3.5)
