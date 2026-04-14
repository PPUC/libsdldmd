#!/bin/bash

set -e

SDL_SHA=5848e584a1b606de26e3dbd1c7e4ecbc34f807a6
LIBDMDUTIL_SHA=85825a2a09f12ddcc4605173058b2e582fe67aac

if [ -z "${BUILD_TYPE}" ]; then
   BUILD_TYPE="Release"
fi

echo "Build type: ${BUILD_TYPE}"
echo ""
