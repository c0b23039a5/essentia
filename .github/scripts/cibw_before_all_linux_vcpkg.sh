#!/usr/bin/env bash
set -euxo pipefail

ROOT="/project"
VCPKG_ROOT="$ROOT/.cibw-cache/vcpkg"
TRIPLET="x64-linux"

if [ ! -d "$VCPKG_ROOT/.git" ]; then
  rm -rf "$VCPKG_ROOT"
  git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
fi

"$VCPKG_ROOT/bootstrap-vcpkg.sh"
"$VCPKG_ROOT/vcpkg" install --triplet "$TRIPLET" --x-manifest-root="$ROOT" --clean-after-build

mkdir -p "$ROOT/.pkgconfig"
cat > "$ROOT/.pkgconfig/eigen3.pc" <<PC
Name: eigen3
Description: Eigen3
Version: 3.4.0
Cflags: -I$VCPKG_ROOT/installed/$TRIPLET/include/eigen3
PC

PYBIN=$(ls -1 /opt/python/cp3*-cp3*/bin/python | head -n 1)
"$PYBIN" waf configure --with-python --pkg-config-path="$ROOT/.pkgconfig:$VCPKG_ROOT/installed/$TRIPLET/lib/pkgconfig:$VCPKG_ROOT/installed/$TRIPLET/share/pkgconfig"
"$PYBIN" waf
"$PYBIN" waf install
