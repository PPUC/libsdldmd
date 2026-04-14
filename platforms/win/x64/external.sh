#!/bin/bash

set -e

source ./platforms/config.sh

echo "Building libraries..."
echo "  SDL_SHA: ${SDL_SHA}"
echo "  LIBDMDUTIL_SHA: ${LIBDMDUTIL_SHA}"
echo ""

rm -rf external
mkdir -p external third-party/include third-party/build-libs/win/x64 third-party/runtime-libs/win/x64
cd external

curl -sL https://github.com/PPUC/libdmdutil/archive/${LIBDMDUTIL_SHA}.tar.gz -o libdmdutil-${LIBDMDUTIL_SHA}.tar.gz
tar xzf libdmdutil-${LIBDMDUTIL_SHA}.tar.gz
mv libdmdutil-${LIBDMDUTIL_SHA} libdmdutil
cp -r libdmdutil/include/DMDUtil ../third-party/include/

curl -sL https://github.com/libsdl-org/SDL/archive/${SDL_SHA}.tar.gz -o SDL-${SDL_SHA}.tar.gz
tar xzf SDL-${SDL_SHA}.tar.gz
mv SDL-${SDL_SHA} SDL
cd SDL
sed -i.bak 's/OUTPUT_NAME "SDL3"/OUTPUT_NAME "SDL364"/g' CMakeLists.txt
cmake \
   -G "Visual Studio 17 2022" \
   -DSDL_SHARED=ON \
   -DSDL_STATIC=OFF \
   -DSDL_TEST_LIBRARY=OFF \
   -B build
cmake --build build --config ${BUILD_TYPE}
cd ..

cp SDL/build/${BUILD_TYPE}/SDL364.lib ../third-party/build-libs/win/x64/
cp SDL/build/${BUILD_TYPE}/SDL364.dll ../third-party/runtime-libs/win/x64/
cp -r SDL/include/SDL3 ../third-party/include/
