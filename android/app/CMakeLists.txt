# OpenWheels - Android native library (libOpenWheels.so), built by the Android Gradle Plugin
# (android/app/build.gradle -> externalNativeBuild) with the NDK's CMake toolchain.
#
# Mirrors the root CMakeLists.txt (Win32) and cocos2d-x 3.17.2's
# templates/cpp-template-default/CMakeLists.txt Android branch:
#   * the engine comes from thirdparty/cocos2d-x through CocosBuildSet + add_subdirectory(cocos),
#     with the v3-deps-158 Android prebuilts (external/*/prebuilt/android/<abi>/);
#   * every source under src/ is globbed, except other operating systems' entry points
#     (src/platform/<os>/) - so src/platform/android/ is in, src/platform/win32/ is out;
#   * cpp_android_spec (javaactivity-android.cpp & co: the JNI entry points Cocos2dxActivity
#     calls) is linked whole-archive, as in the template.
# The game is linked against libc++ (c++_static) like the original libMyGame.so.

cmake_minimum_required(VERSION 3.10)

set(APP_NAME OpenWheels)
project(${APP_NAME} C CXX)

get_filename_component(OW_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(COCOS2DX_ROOT_PATH ${OW_ROOT}/thirdparty/cocos2d-x)
set(CMAKE_MODULE_PATH ${COCOS2DX_ROOT_PATH}/cmake/Modules/)

if(NOT EXISTS "${COCOS2DX_ROOT_PATH}/cocos/cocos2d.h")
    message(FATAL_ERROR "cocos2d-x 3.17.2 not found in ${COCOS2DX_ROOT_PATH}; run tools/fetch_engine.ps1 first.")
endif()
if(NOT ANDROID)
    message(FATAL_ERROR "android/app/CMakeLists.txt is the Android build; use the root CMakeLists.txt for Win32.")
endif()

# Same engine module selection as the Win32 build (see the root CMakeLists.txt).
option(BUILD_LUA_LIBS "Build lua libraries" OFF)
option(BUILD_JS_LIBS "Build js libraries" OFF)
option(BUILD_EXTENSIONS "Build extension library" ON)
option(BUILD_EDITOR_COCOSBUILDER "Build editor support for cocosbuilder" OFF)
option(BUILD_EDITOR_COCOSTUDIO "Build editor support for cocostudio" OFF)
option(BUILD_EDITOR_SPINE "Build editor support for spine" OFF)

include(CocosBuildSet)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# cocos2d-x 3.17.2 predates NDK r2x's clang: keep its warnings out of the log (they are the
# engine's, not ours) and accept the few constructs newer clang made errors by default.
add_compile_options(
    -Wno-deprecated-declarations
    -Wno-overloaded-virtual
    -Wno-inconsistent-missing-override
    -Wno-unused-command-line-argument
    -Wno-deprecated-register
    -Wno-reorder-ctor
    -Wno-unused-variable
    -Wno-unused-private-field
)

add_subdirectory(${COCOS2DX_ROOT_PATH}/cocos ${ENGINE_BINARY_PATH}/cocos/core)

file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
     ${OW_ROOT}/src/*.cpp
     ${OW_ROOT}/src/*.c
     )
foreach(foreign_os win32 ios mac linux winrt tizen emscripten)
    list(FILTER GAME_SOURCES EXCLUDE REGEX "/src/platform/${foreign_os}/")
endforeach()

add_library(${APP_NAME} SHARED ${GAME_SOURCES})

# JNI glue (Cocos2dxRenderer/Cocos2dxHelper/... native methods, JNI_OnLoad): whole-archive, or the
# linker would drop the objects nothing in C++ references.
add_subdirectory(${COCOS2DX_ROOT_PATH}/cocos/platform/android ${ENGINE_BINARY_PATH}/cocos/platform)
target_link_libraries(${APP_NAME} -Wl,--whole-archive cpp_android_spec -Wl,--no-whole-archive)
target_link_libraries(${APP_NAME} cocos2d)

file(GLOB OW_GAME_DIRS LIST_DIRECTORIES true ${OW_ROOT}/src/game/* ${OW_ROOT}/src/editor/*)
list(FILTER OW_GAME_DIRS INCLUDE REGEX "/src/(game|editor)/[^/]+$")
target_include_directories(${APP_NAME}
    PRIVATE ${OW_ROOT}/src
    PRIVATE ${OW_GAME_DIRS}
    PRIVATE ${OW_ROOT}/src/platform/stubs
    PRIVATE ${COCOS2DX_ROOT_PATH}/cocos/audio/include/
)
# No BionicRand.h here: on Android rand()/srand() are bionic's own.

# Android 15+ devices may use 16 KB memory pages: align the segments so the library loads there.
target_link_libraries(${APP_NAME} -Wl,-z,max-page-size=16384)
