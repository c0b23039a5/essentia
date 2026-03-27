#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$SCRIPT_DIR/common.sh"

prepare_build_dir

echo "Building zlib $ZLIB_VERSION"

curl -SLO https://zlib.net/$ZLIB_VERSION.tar.gz
tar -xf $ZLIB_VERSION.tar.gz
cd $ZLIB_VERSION/

CC=$HOST-gcc AR="$HOST-ar" RANLIB=$HOST-ranlib ./configure \
    --prefix=$PREFIX \
    --static
make
make install
