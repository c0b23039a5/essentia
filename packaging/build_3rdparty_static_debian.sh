#!/usr/bin/env bash
set -euo pipefail

BASEDIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
THIRDPARTY_DIR="$BASEDIR/debian_3rdparty"

build_steps=(
  build_eigen3.sh
  build_fftw3.sh
  build_lame.sh
  build_ffmpeg.sh
  build_libsamplerate.sh
  build_zlib.sh
  build_taglib.sh
  build_yaml.sh
  build_chromaprint.sh
)

cd "$THIRDPARTY_DIR"

for step in "${build_steps[@]}"; do
  "./$step"
done

if [[ " $* " == *" --with-gaia "* ]]; then
  ./build_qt.sh
  ./build_gaia.sh
  rm -rf mkspecs plugins translations
fi

if [[ " $* " == *" --with-tensorflow "* ]]; then
  echo "Skipping TensorFlow build (disabled by default in packaging flow)."
fi

rm -rf bin share
