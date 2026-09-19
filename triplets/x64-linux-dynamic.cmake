# MilkDAWp 2 custom triplet: x64 Linux, dynamic linking.
# Forces every vcpkg dependency (notably projectM) to build as a shared library
# (.so), which projectM's LGPL-2.1 licence requires for our AGPL-linked binary.

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
