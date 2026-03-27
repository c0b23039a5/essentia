#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
. "$SCRIPT_DIR/../build_config.sh"

BUILD_TMP_DIR="$SCRIPT_DIR/tmp"

prepare_build_dir() {
  rm -rf "$BUILD_TMP_DIR"
  mkdir -p "$BUILD_TMP_DIR"
  cd "$BUILD_TMP_DIR"
}

cleanup_build_dir() {
  rm -rf "$BUILD_TMP_DIR"
}

trap cleanup_build_dir EXIT
