#!/usr/bin/env bash
set -euxo pipefail

ROOT="$PWD"
VCPKG_ROOT="$ROOT/.cibw-cache/vcpkg"
TRIPLET="${VCPKG_TRIPLET:-arm64-osx}"

if [ ! -d "$VCPKG_ROOT/.git" ]; then
  rm -rf "$VCPKG_ROOT"
  git clone https://github.com/microsoft/vcpkg "$VCPKG_ROOT"
fi

"$VCPKG_ROOT/bootstrap-vcpkg.sh"
"$VCPKG_ROOT/vcpkg" install --triplet "$TRIPLET" --x-manifest-root="$ROOT" --x-install-root="$VCPKG_ROOT/installed" --clean-after-build

mkdir -p "$ROOT/.pkgconfig"
cat > "$ROOT/.pkgconfig/eigen3.pc" <<PC
Name: eigen3
Description: Eigen3
Version: 3.4.0
Cflags: -I$VCPKG_ROOT/installed/$TRIPLET/include/eigen3
PC

cat > "$ROOT/.pkgconfig/yaml-0.1.pc" <<PC
Name: yaml-0.1
Description: libyaml compatibility pkg-config file
Version: 0.2.5
Libs: -L$VCPKG_ROOT/installed/$TRIPLET/lib -lyaml
Cflags: -I$VCPKG_ROOT/installed/$TRIPLET/include
PC

python waf configure --pkg-config-path="$ROOT/.pkgconfig:$VCPKG_ROOT/installed/$TRIPLET/lib/pkgconfig:$VCPKG_ROOT/installed/$TRIPLET/share/pkgconfig"
python waf
python waf install
