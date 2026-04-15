#!/bin/bash

set -e

SDL_SHA=5848e584a1b606de26e3dbd1c7e4ecbc34f807a6
LIBDMDUTIL_SHA=e8215515141131affc5e2c700b6f4b12930bd098

if [ -z "${BUILD_TYPE}" ]; then
   BUILD_TYPE="Release"
fi

echo "Build type: ${BUILD_TYPE}"
echo ""
