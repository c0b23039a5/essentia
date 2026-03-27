#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$SCRIPT_DIR/common.sh"

prepare_build_dir

echo "Building libyaml $LIBYAML_VERSION"

curl -SLO https://github.com/yaml/libyaml/releases/download/${LIBYAML_VERSION#yaml-}/$LIBYAML_VERSION.tar.gz
tar -xf $LIBYAML_VERSION.tar.gz
cd $LIBYAML_VERSION

# fails to compile shared library, building only static
CFLAGS="-DYAML_DECLARE_STATIC" ./configure \
    --host=$HOST \
    --prefix=$PREFIX \
    $SHARED_OR_STATIC
make
make install
