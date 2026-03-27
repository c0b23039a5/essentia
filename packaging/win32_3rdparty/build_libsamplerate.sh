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

cd ../..
rm -r tmp
