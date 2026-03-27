#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$SCRIPT_DIR/common.sh"

prepare_build_dir

echo "Building lame $LAME_VERSION"

curl -SL -o lame-$LAME_VERSION.tar.gz "https://downloads.sourceforge.net/project/lame/lame/$LAME_VERSION/lame-$LAME_VERSION.tar.gz"
tar -xf lame-$LAME_VERSION.tar.gz
cd lame-$LAME_VERSION
CPPFLAGS=-fPIC ./configure \
    --host=$HOST \
    --prefix=$PREFIX \
    $SHARED_OR_STATIC

make
make install
