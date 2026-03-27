#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$SCRIPT_DIR/common.sh"

prepare_build_dir

echo "Building libsamplerate $LIBSAMPLERATE_VERSION"

curl -SLO https://github.com/libsndfile/libsamplerate/releases/download/${LIBSAMPLERATE_VERSION#libsamplerate-}/$LIBSAMPLERATE_VERSION.tar.xz
tar -xf $LIBSAMPLERATE_VERSION.tar.xz
cd $LIBSAMPLERATE_VERSION

./configure \
    --host=$HOST \
    --prefix=$PREFIX \
    --disable-fftw \
    --disable-sndfile \
    $SHARED_OR_STATIC
make
make install

# TODO Unnecessary?
#cp src/.libs/libsamplerate-0.dll $PREFIX/lib/libsamplerate.dll
