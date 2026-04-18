#!/bin/bash

set -e

SDL_SHA=5848e584a1b606de26e3dbd1c7e4ecbc34f807a6
LIBDMDUTIL_SHA=b09fbab3d60e4d78629d8fd3dc3742d460644d45

if [ -z "${BUILD_TYPE}" ]; then
   BUILD_TYPE="Release"
fi

echo "Build type: ${BUILD_TYPE}"
echo ""
