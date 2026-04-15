#!/bin/bash

set -e

SDL_SHA=5848e584a1b606de26e3dbd1c7e4ecbc34f807a6
LIBDMDUTIL_SHA=b8b0bea2682787c8f5f0d2c165c146f79eec3778

if [ -z "${BUILD_TYPE}" ]; then
   BUILD_TYPE="Release"
fi

echo "Build type: ${BUILD_TYPE}"
echo ""
