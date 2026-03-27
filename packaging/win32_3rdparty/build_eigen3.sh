#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$SCRIPT_DIR/common.sh"

prepare_build_dir

echo "Installing headers for Eigen $EIGEN_VERSION"

curl -SLO https://gitlab.com/libeigen/eigen/-/archive/$EIGEN_VERSION/eigen-$EIGEN_VERSION.tar.gz
tar -xf eigen-$EIGEN_VERSION.tar.gz
cd eigen-$EIGEN_VERSION

mkdir build
cd build

cmake ../ -DCMAKE_INSTALL_PREFIX=$PREFIX
make install
mkdir -p "$PREFIX"/lib/pkgconfig/
cp "$PREFIX"/share/pkgconfig/eigen3.pc "$PREFIX"/lib/pkgconfig/
