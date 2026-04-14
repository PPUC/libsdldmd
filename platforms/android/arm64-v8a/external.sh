#!/bin/bash

set -e

source ./platforms/config.sh

if [[ $(uname) == "Linux" ]]; then
   NUM_PROCS=$(nproc)
elif [[ $(uname) == "Darwin" ]]; then
   NUM_PROCS=$(sysctl -n hw.ncpu)
else
   NUM_PROCS=1
fi

echo "Building libraries..."
echo "  SDL_SHA: ${SDL_SHA}"
echo "  LIBDMDUTIL_SHA: ${LIBDMDUTIL_SHA}"
echo ""

rm -rf external
mkdir -p external third-party/include third-party/runtime-libs/android/arm64-v8a
cd external

curl -sL https://github.com/PPUC/libdmdutil/archive/${LIBDMDUTIL_SHA}.tar.gz -o libdmdutil-${LIBDMDUTIL_SHA}.tar.gz
tar xzf libdmdutil-${LIBDMDUTIL_SHA}.tar.gz
mv libdmdutil-${LIBDMDUTIL_SHA} libdmdutil
cd libdmdutil
BUILD_TYPE=${BUILD_TYPE} platforms/android/arm64-v8a/external.sh
cmake \
   -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_LATEST_HOME}/build/cmake/android.toolchain.cmake" \
   -DPLATFORM=android \
   -DARCH=arm64-v8a \
   -DBUILD_SHARED=ON \
   -DBUILD_STATIC=OFF \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cp -r include/DMDUtil ../../third-party/include/
cp build/libdmdutil.so ../../third-party/runtime-libs/android/arm64-v8a/
cd ..

curl -sL https://github.com/libsdl-org/SDL/archive/${SDL_SHA}.tar.gz -o SDL-${SDL_SHA}.tar.gz
tar xzf SDL-${SDL_SHA}.tar.gz
mv SDL-${SDL_SHA} SDL
cd SDL
cmake \
   -DSDL_SHARED=ON \
   -DSDL_STATIC=OFF \
   -DSDL_TEST_LIBRARY=OFF \
   -DCMAKE_TOOLCHAIN_FILE="${ANDROID_NDK_LATEST_HOME}/build/cmake/android.toolchain.cmake" \
   -DCMAKE_SYSTEM_NAME=Android \
   -DCMAKE_SYSTEM_VERSION=30 \
   -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build -- -j${NUM_PROCS}
cd ..

cp SDL/build/libSDL3.so ../third-party/runtime-libs/android/arm64-v8a/
cp -r SDL/include/SDL3 ../third-party/include/
