#!/bin/bash

set -e

SDL_SHA=8e37db5e797b6167f3a00d697d816a684bd259c7
LIBDMDUTIL_SHA=734a0bddc05f72bd560966a1b74b982c2191d976

if [ -z "${BUILD_TYPE}" ]; then
   BUILD_TYPE="Release"
fi

echo "Build type: ${BUILD_TYPE}"
echo ""
