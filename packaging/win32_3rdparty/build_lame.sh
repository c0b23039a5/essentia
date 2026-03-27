#!/usr/bin/env bash
set -e
. ../build_config.sh

rm -rf tmp
mkdir tmp
cd tmp

echo "Building lame $LAME_VERSION"

#!/usr/bin/env bash
curl -SL -o lame-$LAME_VERSION.tar.gz "https://downloads.sourceforge.net/project/lame/lame/$LAME_VERSION/lame-$LAME_VERSION.tar.gz"
tar -xf  lame-$LAME_VERSION.tar.gz
cd lame-$LAME_VERSION
CPPFLAGS=-fPIC ./configure \
    --host=$HOST \
    --prefix=$PREFIX \
    $SHARED_OR_STATIC

make
make install

cd ../..
rm -r tmp
