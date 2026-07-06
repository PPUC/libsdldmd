#!/bin/bash

set -e

source ./platforms/config.sh

NUM_PROCS=$(nproc)

echo "Building libraries..."
echo "  SDL_SHA: ${SDL_SHA}"
echo "  LIBDMDUTIL_SHA: ${LIBDMDUTIL_SHA}"
print_dependency_source LIBDMDUTIL "${LIBDMDUTIL_SHA}" LIBDMDUTIL_SOURCE_DIR
echo ""

rm -rf external
mkdir -p external third-party/include third-party/runtime-libs/linux/x64
cd external

prepare_dependency_source libdmdutil "${LIBDMDUTIL_SHA}" "https://github.com/PPUC/libdmdutil/archive/${LIBDMDUTIL_SHA}.tar.gz" tar LIBDMDUTIL_SOURCE_DIR
cd libdmdutil
BUILD_TYPE=${BUILD_TYPE} platforms/linux/x64/external.sh
cmake \
   -DPLATFORM=linux \
   -DARCH=x64 \
   -DBUILD_SHARED=ON \
   -DBUILD_STATIC=OFF \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cp -r include/DMDUtil ${PROJECT_SOURCE_ROOT}/third-party/include/
cp -a build/libdmdutil.{so,so.*} ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/linux/x64/
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
   -DSDL_WAYLAND=OFF \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cd ..

cp -a SDL/build/libSDL3.{so,so.*} ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/linux/x64/
cp -r SDL/include/SDL3 ${PROJECT_SOURCE_ROOT}/third-party/include/
