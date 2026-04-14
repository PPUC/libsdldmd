#!/bin/bash

set -e

source ./platforms/config.sh

NUM_PROCS=$(sysctl -n hw.ncpu)

echo "Building libraries..."
echo "  SDL_SHA: ${SDL_SHA}"
echo "  LIBDMDUTIL_SHA: ${LIBDMDUTIL_SHA}"
echo ""

rm -rf external
mkdir -p external third-party/include third-party/build-libs/ios/arm64
cd external

curl -sL https://github.com/PPUC/libdmdutil/archive/${LIBDMDUTIL_SHA}.tar.gz -o libdmdutil-${LIBDMDUTIL_SHA}.tar.gz
tar xzf libdmdutil-${LIBDMDUTIL_SHA}.tar.gz
mv libdmdutil-${LIBDMDUTIL_SHA} libdmdutil
cd libdmdutil
BUILD_TYPE=${BUILD_TYPE} platforms/ios/arm64/external.sh
cmake \
   -DPLATFORM=ios \
   -DARCH=arm64 \
   -DBUILD_SHARED=OFF \
   -DBUILD_STATIC=ON \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cp -r include/DMDUtil ../../third-party/include/
cp build/libdmdutil.a ../../third-party/build-libs/ios/arm64/
cd ..

curl -sL https://github.com/libsdl-org/SDL/archive/${SDL_SHA}.tar.gz -o SDL-${SDL_SHA}.tar.gz
tar xzf SDL-${SDL_SHA}.tar.gz
mv SDL-${SDL_SHA} SDL
cd SDL
cmake \
   -DSDL_SHARED=OFF \
   -DSDL_STATIC=ON \
   -DSDL_TEST_LIBRARY=OFF \
   -DCMAKE_SYSTEM_NAME=iOS \
   -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
   -DCMAKE_OSX_ARCHITECTURES=arm64 \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cd ..

cp SDL/build/libSDL3.a ../third-party/build-libs/ios/arm64/
cp -r SDL/include/SDL3 ../third-party/include/
