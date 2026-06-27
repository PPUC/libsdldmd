#!/bin/bash

set -e

source ./platforms/config.sh

echo "Building libraries..."
echo "  SDL_SHA: ${SDL_SHA}"
echo "  LIBDMDUTIL_SHA: ${LIBDMDUTIL_SHA}"
ppuc_print_dependency_source LIBDMDUTIL libdmdutil "${LIBDMDUTIL_SHA}"
echo ""

rm -rf external
mkdir -p external third-party/include third-party/build-libs/win/x64 third-party/runtime-libs/win/x64
cd external

ppuc_prepare_dependency_source libdmdutil "${LIBDMDUTIL_SHA}" "https://github.com/PPUC/libdmdutil/archive/${LIBDMDUTIL_SHA}.tar.gz"
cd libdmdutil
BUILD_TYPE=${BUILD_TYPE} platforms/win/x64/external.sh
cmake \
   -G "Visual Studio 17 2022" \
   -DPLATFORM=win \
   -DARCH=x64 \
   -DBUILD_SHARED=ON \
   -DBUILD_STATIC=OFF \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -B build
cmake --build build --config ${BUILD_TYPE}
cp -r include/DMDUtil ${PPUC_SOURCE_ROOT}/third-party/include/
cp build/${BUILD_TYPE}/dmdutil64.lib ${PPUC_SOURCE_ROOT}/third-party/build-libs/win/x64/
cp build/${BUILD_TYPE}/dmdutil64.dll ${PPUC_SOURCE_ROOT}/third-party/runtime-libs/win/x64/
cd ..

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

cp SDL/build/${BUILD_TYPE}/SDL364.lib ${PPUC_SOURCE_ROOT}/third-party/build-libs/win/x64/
cp SDL/build/${BUILD_TYPE}/SDL364.dll ${PPUC_SOURCE_ROOT}/third-party/runtime-libs/win/x64/
cp -r SDL/include/SDL3 ${PPUC_SOURCE_ROOT}/third-party/include/
