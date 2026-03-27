#!/usr/bin/env bash
set -euo pipefail

BASEDIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
THIRDPARTY_DIR="$BASEDIR/win32_3rdparty"

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
rm -rf bin dynamic include lib share

for step in "${build_steps[@]}"; do
  "./$step"
done

rm -rf bin dynamic share
