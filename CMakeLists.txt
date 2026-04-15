cmake_minimum_required(VERSION 3.25)

set(PLATFORM "win" CACHE STRING "Platform")
set(ARCH "x64" CACHE STRING "Arch")
option(BUILD_SHARED "Option to build shared library" ON)
option(BUILD_STATIC "Option to build static library" ON)

message(STATUS "PLATFORM: ${PLATFORM}")
message(STATUS "ARCH: ${ARCH}")
message(STATUS "BUILD_SHARED: ${BUILD_SHARED}")
message(STATUS "BUILD_STATIC: ${BUILD_STATIC}")

if(PLATFORM STREQUAL "ios" OR PLATFORM STREQUAL "ios-simulator")
   set(CMAKE_SYSTEM_NAME iOS)
   if(PLATFORM STREQUAL "ios-simulator")
      set(CMAKE_OSX_SYSROOT iphonesimulator)
   endif()
   set(CMAKE_OSX_DEPLOYMENT_TARGET 17.0)
elseif(PLATFORM STREQUAL "tvos")
   set(CMAKE_SYSTEM_NAME tvOS)
   set(CMAKE_OSX_DEPLOYMENT_TARGET 17.0)
elseif(PLATFORM STREQUAL "android")
   if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{ANDROID_NDK_HOME})
      set(CMAKE_TOOLCHAIN_FILE "$ENV{ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake")
   endif()
   set(CMAKE_SYSTEM_NAME Android)
   set(CMAKE_SYSTEM_VERSION 30)
   set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
endif()

file(READ include/SDLDMD/SDLDMD.h version)
string(REGEX MATCH "#[ \t]*define[ \t]+SDLDMD_VERSION_MAJOR[ \t]+([0-9]+)" _tmp "${version}")
if("${CMAKE_MATCH_1}" STREQUAL "")
   message(FATAL_ERROR "Failed to parse SDLDMD_VERSION_MAJOR from include/SDLDMD/SDLDMD.h")
endif()
set(VERSION_MAJOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "#[ \t]*define[ \t]+SDLDMD_VERSION_MINOR[ \t]+([0-9]+)" _tmp "${version}")
if("${CMAKE_MATCH_1}" STREQUAL "")
   message(FATAL_ERROR "Failed to parse SDLDMD_VERSION_MINOR from include/SDLDMD/SDLDMD.h")
endif()
set(VERSION_MINOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "#[ \t]*define[ \t]+SDLDMD_VERSION_PATCH[ \t]+([0-9]+)" _tmp "${version}")
if("${CMAKE_MATCH_1}" STREQUAL "")
   message(FATAL_ERROR "Failed to parse SDLDMD_VERSION_PATCH from include/SDLDMD/SDLDMD.h")
endif()
set(VERSION_PATCH "${CMAKE_MATCH_1}")

project(libsdldmd VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}" LANGUAGES CXX)

if(PLATFORM STREQUAL "win")
   if(ARCH STREQUAL "x86")
      add_compile_definitions(WIN32)
   endif()
elseif(PLATFORM STREQUAL "win-mingw")
   add_compile_options(-Wa,-mbig-obj)
elseif(PLATFORM STREQUAL "macos")
   if(ARCH STREQUAL "arm64")
      set(CMAKE_OSX_ARCHITECTURES arm64)
   elseif(ARCH STREQUAL "x64")
      set(CMAKE_OSX_ARCHITECTURES x86_64)
   endif()
   set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
   set(CMAKE_INSTALL_RPATH "@executable_path")
elseif(PLATFORM STREQUAL "ios" OR PLATFORM STREQUAL "ios-simulator" OR PLATFORM STREQUAL "tvos")
   set(CMAKE_OSX_ARCHITECTURES arm64)
elseif(PLATFORM STREQUAL "linux")
   set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
   set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third-party/include/DMDUtil/RGB24DMD.h")
   message(FATAL_ERROR "Could not find staged DMDUtil headers in third-party/include/DMDUtil.")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third-party/include/SDL3/SDL.h")
   message(FATAL_ERROR "Could not find staged SDL3 headers in third-party/include/SDL3. Run the platform external script first.")
endif()

set(SDLDMD_SOURCES
   src/SDLDMD.cpp
   src/xbrz/xbrz.cpp
)

set(SDLDMD_INCLUDE_DIRS
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/third-party/include
)

function(sdldmd_apply_link_settings target_name)
   if(PLATFORM STREQUAL "win")
      target_link_directories(${target_name} PUBLIC
         third-party/build-libs/${PLATFORM}/${ARCH}
         third-party/runtime-libs/${PLATFORM}/${ARCH}
      )
      if(ARCH STREQUAL "x64")
         target_link_libraries(${target_name} PUBLIC dmdutil64 SDL364)
      else()
         target_link_libraries(${target_name} PUBLIC dmdutil SDL3)
      endif()
   elseif(PLATFORM STREQUAL "win-mingw")
      target_link_directories(${target_name} PUBLIC
         third-party/build-libs/${PLATFORM}/${ARCH}
         third-party/runtime-libs/${PLATFORM}/${ARCH}
      )
      target_link_libraries(${target_name} PUBLIC dmdutil64 SDL364)
   elseif(PLATFORM STREQUAL "macos" OR PLATFORM STREQUAL "linux" OR PLATFORM STREQUAL "android")
      target_link_directories(${target_name} PUBLIC
         third-party/runtime-libs/${PLATFORM}/${ARCH}
      )
      target_link_libraries(${target_name} PUBLIC dmdutil SDL3)
   elseif(PLATFORM STREQUAL "ios" OR PLATFORM STREQUAL "ios-simulator" OR PLATFORM STREQUAL "tvos")
      target_link_directories(${target_name} PUBLIC
         third-party/build-libs/${PLATFORM}/${ARCH}
      )
      target_link_libraries(${target_name} PUBLIC dmdutil SDL3)
   endif()
endfunction()

if(BUILD_SHARED)
   add_library(sdldmd_shared SHARED ${SDLDMD_SOURCES})
   target_include_directories(sdldmd_shared PUBLIC ${SDLDMD_INCLUDE_DIRS} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
   sdldmd_apply_link_settings(sdldmd_shared)

   if((PLATFORM STREQUAL "win" OR PLATFORM STREQUAL "win-mingw") AND ARCH STREQUAL "x64")
      set(SDLDMD_OUTPUT_NAME "sdldmd64")
   else()
      set(SDLDMD_OUTPUT_NAME "sdldmd")
   endif()

   set_target_properties(sdldmd_shared PROPERTIES
      OUTPUT_NAME ${SDLDMD_OUTPUT_NAME}
      POSITION_INDEPENDENT_CODE ON
      VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
   )
   if(PLATFORM STREQUAL "win-mingw")
      set_target_properties(sdldmd_shared PROPERTIES
         PREFIX ""
         IMPORT_PREFIX ""
      )
   endif()

   install(TARGETS sdldmd_shared
      LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
   )
endif()

if(BUILD_STATIC)
   add_library(sdldmd_static STATIC ${SDLDMD_SOURCES})
   target_include_directories(sdldmd_static PUBLIC ${SDLDMD_INCLUDE_DIRS} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
   sdldmd_apply_link_settings(sdldmd_static)

   if(PLATFORM STREQUAL "win" OR PLATFORM STREQUAL "win-mingw")
      set_target_properties(sdldmd_static PROPERTIES
         PREFIX ""
         OUTPUT_NAME "sdldmd_static"
      )
   else()
      set_target_properties(sdldmd_static PROPERTIES
         OUTPUT_NAME "sdldmd"
      )
   endif()

   install(TARGETS sdldmd_static
      ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
   )
endif()

install(DIRECTORY include/SDLDMD DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
