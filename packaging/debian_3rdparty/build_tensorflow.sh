#!/usr/bin/env bash
set -e
. ../build_config.sh

TENSORFLOW_BUILD_VARIANT="${1:-auto}"
case "${TENSORFLOW_BUILD_VARIANT}" in
  cpu)
    export TF_NEED_CUDA=0
    ;;
  gpu)
    export TF_NEED_CUDA=1
    ;;
  auto)
    ;;
  *)
    echo "Usage: $0 [auto|cpu|gpu]"
    exit 1
    ;;
esac

rm -rf tmp
mkdir tmp
cd tmp


echo "Building Tensorflow $TENSORFLOW_VERSION (variant: ${TENSORFLOW_BUILD_VARIANT}, TF_NEED_CUDA=${TF_NEED_CUDA})"

curl -SLO https://github.com/tensorflow/tensorflow/archive/v$TENSORFLOW_VERSION.tar.gz
tar -xf v$TENSORFLOW_VERSION.tar.gz
cd tensorflow-$TENSORFLOW_VERSION

yes '' | ./configure

bazel build //tensorflow/tools/lib_package:libtensorflow ${TENSORFLOW_FLAGS}

tar xzf bazel-bin/tensorflow/tools/lib_package/libtensorflow.tar.gz -C ${PREFIX}

./tensorflow/c/generate-pc.sh -p ${PREFIX} -v $TENSORFLOW_VERSION

# Patch the pkg-config file to remove the dependency on libtensorflow_framework.so,
# which is not required in monolithic builds.
sed -i 's/ -ltensorflow_framework//' tensorflow.pc

cp tensorflow.pc ${PREFIX}/lib/pkgconfig/

# Clean Bazel's cache (~9GB)
bazel clean --expunge
rm -rf /root/.cache/bazel*

cd ../..
rm -r tmp
