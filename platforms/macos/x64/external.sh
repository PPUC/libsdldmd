#!/bin/bash

set -e

source ./platforms/config.sh

export MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-${PPUC_MACOS_DEPLOYMENT_TARGET:-$(xcrun --sdk macosx --show-sdk-version 2>/dev/null || echo 14.0)}}"
NUM_PROCS=$(sysctl -n hw.ncpu)

echo "Building libraries..."
echo "  SDL_SHA: ${SDL_SHA}"
echo "  LIBDMDUTIL_SHA: ${LIBDMDUTIL_SHA}"
ppuc_print_dependency_source LIBDMDUTIL libdmdutil "${LIBDMDUTIL_SHA}"
echo ""

rm -rf external
mkdir -p external third-party/include third-party/runtime-libs/macos/x64
cd external

ppuc_prepare_dependency_source libdmdutil "${LIBDMDUTIL_SHA}" "https://github.com/PPUC/libdmdutil/archive/${LIBDMDUTIL_SHA}.tar.gz"
cd libdmdutil
BUILD_TYPE=${BUILD_TYPE} platforms/macos/x64/external.sh
cmake \
   -DPLATFORM=macos \
   -DARCH=x64 \
   -DBUILD_SHARED=ON \
   -DBUILD_STATIC=OFF \
   -DCMAKE_OSX_ARCHITECTURES=x86_64 \
   -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cp -r include/DMDUtil ${PPUC_SOURCE_ROOT}/third-party/include/
cp -a build/libdmdutil.{dylib,*.dylib} ${PPUC_SOURCE_ROOT}/third-party/runtime-libs/macos/x64/
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
   -DCMAKE_OSX_ARCHITECTURES=x86_64 \
   -DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cd ..

cp -a SDL/build/libSDL3.{dylib,*.dylib} ${PPUC_SOURCE_ROOT}/third-party/runtime-libs/macos/x64/
cp -r SDL/include/SDL3 ${PPUC_SOURCE_ROOT}/third-party/include/
