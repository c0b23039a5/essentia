#!/usr/bin/env bash
set -e
. ../build_config.sh

rm -rf tmp
mkdir tmp
cd tmp

echo "Building libsamplerate $LIBSAMPLERATE_VERSION"

curl -SLO https://github.com/libsndfile/libsamplerate/releases/download/${LIBSAMPLERATE_VERSION#libsamplerate-}/$LIBSAMPLERATE_VERSION.tar.xz
tar -xf $LIBSAMPLERATE_VERSION.tar.xz
cd $LIBSAMPLERATE_VERSION

CPPFLAGS=-fPIC ./configure \
    --prefix=$PREFIX \
    $LIBSAMPLERATE_FLAGS \
    $SHARED_OR_STATIC
make
make install

cd ../..
rm -r tmp
