#!/bin/bash

set -e

# Overridable, because an embedding host has to win.
#
# libsdldmd renders a DMD through SDL, and a host that embeds it links SDL too.
# If the two fetch different revisions there are two SDL3 libraries in one
# process, which is not a thing that fails cleanly. So the host passes the
# revision it is building everything else against -- ppuc takes vpinball's,
# since vpinball's plugins run in the same process -- and this value is only
# the default for building libsdldmd on its own.
SDL_SHA="${SDL_SHA:-829a65d769d935c4852f8159e964312c0957260a}"
LIBDMDUTIL_SHA=6bd819c97c6918bd0fc799d2f2eb2dfa73008a00

PROJECT_SOURCE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)"

dependency_source_dir() {
   local var_name="$1"
   local source_dir="${!var_name:-}"

   if [ -z "${source_dir}" ]; then
      return 0
   fi

   (cd "${PROJECT_SOURCE_ROOT}" && cd "${source_dir}" && pwd -P)
}

print_dependency_source() {
   local label="$1"
   local sha="$2"
   local source_var="$3"
   local source_dir

   source_dir="$(dependency_source_dir "${source_var}")"
   if [ -n "${source_dir}" ]; then
      echo "  ${label}_SOURCE_DIR: ${source_dir}"
   else
      echo "  ${label}_SOURCE: archive ${sha}"
   fi
}

prepare_dependency_source() {
   local name="$1"
   local sha="$2"
   local url="$3"
   local archive_type="${4:-tar}"
   local source_var="$5"
   local source_dir

   source_dir="$(dependency_source_dir "${source_var}")"
   if [ -n "${source_dir}" ]; then
      echo "Using ${source_var}: ${source_dir}"
      ln -s "${source_dir}" "${name}"
   elif [ "${archive_type}" = "zip" ]; then
      curl -sL "${url}" -o "${name}.zip"
      unzip "${name}.zip"
      mv "${name}-${sha}" "${name}"
   else
      curl -sL "${url}" -o "${name}-${sha}.tar.gz"
      tar xzf "${name}-${sha}.tar.gz"
      mv "${name}-${sha}" "${name}"
   fi
}



if [ -z "${BUILD_TYPE}" ]; then
   BUILD_TYPE="Release"
fi

echo "Build type: ${BUILD_TYPE}"
echo ""
