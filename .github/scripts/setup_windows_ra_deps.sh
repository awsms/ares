#!/usr/bin/env bash
set -euo pipefail

deps_dir="$PWD/.deps/ares-deps-windows-x64"
archive="$PWD/.deps/ares-deps-windows-x64.tar.xz"
deps_sha256=2a962a3563907e796ee9f010f5e65f518fc9af890d1f90d8d0fbfd94773bf8d0
rcheevos_commit=1433173220a7eaede6a9ed7a18e94117be1821e0

mkdir -p .deps
curl --fail --location --retry 3 --output "$archive" \
  https://github.com/awsms/ares-deps/releases/download/2026-03-17/ares-deps-windows-x64.tar.xz
echo "$deps_sha256  $archive" | sha256sum --check --status
tar -xJf "$archive" -C .deps
test -f "$deps_dir/lib/libcurl.lib"
test -f "$deps_dir/lib/libcurl.dll"
test -f "$deps_dir/include/curl/curl.h"

# The published fork package has libcurl, but predates the fork's rcheevos recipe.
# Supply the same pinned rcheevos revision as that recipe until a package is released.
git init --quiet .deps/rcheevos-source
git -C .deps/rcheevos-source remote add origin https://github.com/RetroAchievements/rcheevos.git
git -C .deps/rcheevos-source fetch --depth 1 origin "$rcheevos_commit"
git -C .deps/rcheevos-source checkout --detach FETCH_HEAD
test "$(git -C .deps/rcheevos-source rev-parse HEAD)" = "$rcheevos_commit"

cmake -S .github/scripts/ra-rcheevos -B build_rcheevos -G "Visual Studio 18 2026" \
  -DRCHEEVOS_SOURCE_DIR="$PWD/.deps/rcheevos-source" \
  -DCMAKE_INSTALL_PREFIX="$deps_dir" \
  -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build_rcheevos --target rcheevos --config RelWithDebInfo
cmake --install build_rcheevos --config RelWithDebInfo

# This older release does not ship a CURLConfig.cmake required by desktop-ui.
mkdir -p "$deps_dir/lib/cmake/CURL"
cat > "$deps_dir/lib/cmake/CURL/CURLConfig.cmake" <<'EOF'
get_filename_component(_ares_ra_deps_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT TARGET CURL::libcurl)
  add_library(CURL::libcurl UNKNOWN IMPORTED)
  set_target_properties(CURL::libcurl PROPERTIES
    IMPORTED_LOCATION "${_ares_ra_deps_prefix}/lib/libcurl.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${_ares_ra_deps_prefix}/include"
  )
endif()
unset(_ares_ra_deps_prefix)
EOF

test -f "$deps_dir/lib/cmake/rcheevos/rcheevosConfig.cmake"
test -f "$deps_dir/lib/cmake/CURL/CURLConfig.cmake"
