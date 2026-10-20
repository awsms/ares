#!/usr/bin/env bash
set -euo pipefail

deps_dir="$PWD/.deps/ares-deps-windows-x64"
archive="$PWD/.deps/ares-deps-windows-x64.tar.xz"
deps_sha256=d9aee832f72f15c97651ee1e511830efa4ed4f0390995bdc54276397e3cbaecf

mkdir -p .deps
curl --fail --location --retry 3 --output "$archive" \
  https://github.com/awsms/ares-deps/releases/download/2026-10-20-ra-2/ares-deps-windows-x64.tar.xz
echo "$deps_sha256  $archive" | sha256sum --check --status
tar -xJf "$archive" -C .deps
test -f "$deps_dir/lib/libcurl.lib"
test -f "$deps_dir/lib/rcheevos.lib"
test -f "$deps_dir/include/curl/curl.h"
test -f "$deps_dir/include/rcheevos/rc_client_raintegration.h"
test -f "$deps_dir/lib/cmake/rcheevos/rcheevosConfig.cmake"
test -f "$deps_dir/lib/cmake/CURL/CURLConfig.cmake"
grep -Fq RC_CLIENT_SUPPORTS_RAINTEGRATION "$deps_dir/lib/cmake/rcheevos/rcheevosTargets.cmake"
