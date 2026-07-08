#!/bin/bash

set -e

SDL_SHA=f87239e71e42da91ca317a12eefb82cfbf3393eb
LIBDMDUTIL_SHA=53b6db7b997bedbeade8e3589f43f676efeddb3d

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
