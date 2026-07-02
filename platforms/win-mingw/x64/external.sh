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
mkdir -p external third-party/include third-party/build-libs/win-mingw/x64 third-party/runtime-libs/win-mingw/x64
cd external

prepare_dependency_source libdmdutil "${LIBDMDUTIL_SHA}" "https://github.com/PPUC/libdmdutil/archive/${LIBDMDUTIL_SHA}.tar.gz" tar LIBDMDUTIL_SOURCE_DIR
cd libdmdutil
BUILD_TYPE=${BUILD_TYPE} platforms/win-mingw/x64/external.sh
cmake \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -DPLATFORM=win-mingw \
   -DARCH=x64 \
   -DBUILD_SHARED=ON \
   -DBUILD_STATIC=OFF \
   -B build
cmake --build build -- -j${NUM_PROCS}
cp -r include/DMDUtil ${PROJECT_SOURCE_ROOT}/third-party/include/
cp build/dmdutil64.dll.a ${PROJECT_SOURCE_ROOT}/third-party/build-libs/win-mingw/x64/
cp build/dmdutil64.dll ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/win-mingw/x64/
cd ..

curl -sL https://github.com/libsdl-org/SDL/archive/${SDL_SHA}.tar.gz -o SDL-${SDL_SHA}.tar.gz
tar xzf SDL-${SDL_SHA}.tar.gz
mv SDL-${SDL_SHA} SDL
cd SDL
sed -i.bak 's/OUTPUT_NAME "SDL3"/OUTPUT_NAME "SDL364"/g' CMakeLists.txt
cmake \
   -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
   -DSDL_SHARED=ON \
   -DSDL_STATIC=OFF \
   -DSDL_TEST_LIBRARY=OFF \
   -B build
cmake --build build -- -j${NUM_PROCS}
cd ..

cp SDL/build/libSDL364.dll.a ${PROJECT_SOURCE_ROOT}/third-party/build-libs/win-mingw/x64/
cp SDL/build/SDL364.dll ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/win-mingw/x64/
cp -r SDL/include/SDL3 ${PROJECT_SOURCE_ROOT}/third-party/include/

UCRT64_BIN="${MINGW_PREFIX}/bin"
cp "${UCRT64_BIN}/libgcc_s_seh-1.dll" ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/win-mingw/x64/
cp "${UCRT64_BIN}/libstdc++-6.dll" ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/win-mingw/x64/
cp "${UCRT64_BIN}/libwinpthread-1.dll" ${PROJECT_SOURCE_ROOT}/third-party/runtime-libs/win-mingw/x64/
