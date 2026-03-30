#!/usr/bin/env bash
set -euxo pipefail

ROOT="/project"
VCPKG_ROOT="$ROOT/.cibw-cache/vcpkg"
TRIPLET="${VCPKG_TRIPLET:-x64-linux-dynamic}"

ensure_toolchain_prereqs() {
  if command -v zip >/dev/null 2>&1 && command -v unzip >/dev/null 2>&1 && command -v tar >/dev/null 2>&1 && command -v curl >/dev/null 2>&1 && command -v nasm >/dev/null 2>&1; then
    return
  fi

  if command -v apt-get >/dev/null 2>&1; then
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y curl zip unzip tar nasm
  elif command -v dnf >/dev/null 2>&1; then
    dnf install -y curl zip unzip tar nasm
  elif command -v microdnf >/dev/null 2>&1; then
    microdnf install -y curl zip unzip tar nasm
  elif command -v yum >/dev/null 2>&1; then
    yum install -y curl zip unzip tar nasm
  elif command -v zypper >/dev/null 2>&1; then
    zypper --non-interactive install curl zip unzip tar nasm
  elif command -v apk >/dev/null 2>&1; then
    apk add --no-cache curl zip unzip tar nasm
    export VCPKG_FORCE_SYSTEM_BINARIES=1
  else
    echo "Unable to install vcpkg prerequisites automatically (need curl/zip/unzip/tar/nasm)." >&2
    exit 1
  fi
}

ensure_toolchain_prereqs

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

export CXXFLAGS="${CXXFLAGS:-} -std=c++14"
PREFIX_DIR="$ROOT/.cibw-prefix"
mkdir -p "$PREFIX_DIR"

PYBIN=$(ls -1 /opt/python/cp3*-cp3*/bin/python | head -n 1)
"$PYBIN" waf configure --prefix="$PREFIX_DIR" --pkg-config-path="$ROOT/.pkgconfig:$VCPKG_ROOT/installed/$TRIPLET/lib/pkgconfig:$VCPKG_ROOT/installed/$TRIPLET/share/pkgconfig"
"$PYBIN" waf
"$PYBIN" waf install
