#!/bin/bash

set -e

source ./platforms/config.sh

NUM_PROCS=$(nproc)

echo "Building libraries..."
echo "  SDL_SHA: ${SDL_SHA}"
echo "  LIBDMDUTIL_SHA: ${LIBDMDUTIL_SHA}"
echo ""

rm -rf external
mkdir -p external third-party/include third-party/runtime-libs/linux/aarch64
cd external

curl -sL https://github.com/PPUC/libdmdutil/archive/${LIBDMDUTIL_SHA}.tar.gz -o libdmdutil-${LIBDMDUTIL_SHA}.tar.gz
tar xzf libdmdutil-${LIBDMDUTIL_SHA}.tar.gz
mv libdmdutil-${LIBDMDUTIL_SHA} libdmdutil
cd libdmdutil
BUILD_TYPE=${BUILD_TYPE} platforms/linux/aarch64/external.sh
cmake \
   -DPLATFORM=linux \
   -DARCH=aarch64 \
   -DBUILD_SHARED=ON \
   -DBUILD_STATIC=OFF \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cp -r include/DMDUtil ../../third-party/include/
cp -a build/libdmdutil.{so,so.*} ../../third-party/runtime-libs/linux/aarch64/
cd ..

curl -sL https://github.com/libsdl-org/SDL/archive/${SDL_SHA}.tar.gz -o SDL-${SDL_SHA}.tar.gz
tar xzf SDL-${SDL_SHA}.tar.gz
mv SDL-${SDL_SHA} SDL
cd SDL
cmake \
   -DSDL_SHARED=ON \
   -DSDL_STATIC=OFF \
   -DSDL_TEST_LIBRARY=OFF \
   -DSDL_OPENGLES=OFF \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cd ..

cp -a SDL/build/libSDL3.{so,so.*} ../third-party/runtime-libs/linux/aarch64/
cp -r SDL/include/SDL3 ../third-party/include/
