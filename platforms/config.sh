#!/bin/bash

set -e

SDL_SHA=5848e584a1b606de26e3dbd1c7e4ecbc34f807a6
LIBDMDUTIL_SHA=3b66a4fa487c6873da29d51a641c7b2431de107f

if [ -z "${BUILD_TYPE}" ]; then
   BUILD_TYPE="Release"
fi

echo "Build type: ${BUILD_TYPE}"
echo ""
