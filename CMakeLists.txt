# OpenWheels - native Windows (Win32 / x86) build on top of cocos2d-x 3.17.2.
#
# Modeled on cocos2d-x 3.17.2 templates/cpp-template-default/CMakeLists.txt:
# the engine is pulled in through its own CocosBuildSet machinery and
# add_subdirectory(cocos), and the app links the static `cocos2d` target.
#
# Engine: thirdparty/cocos2d-x = unmodified tag cocos2d-x-3.17.2 + v3-deps-158
# prebuilts (it builds with MSVC 14.44 / v143 without source patches; any
# thirdparty/patches/*.patch would be applied by the fetch script) - set it up
# with tools/fetch_engine.ps1, build with tools/build.ps1.
#
# The v3-deps-158 win32 prebuilts are 32-bit only, so configure with -A Win32
# (tools/build.ps1 does this; pin the VS 2022 instance, or VS 18's bundled
# CMake hands the 2022 generator to VS 18's MSBuild, which rejects v143):
#   cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 -T v143
#         "-DCMAKE_GENERATOR_INSTANCE=C:/Program Files/Microsoft Visual Studio/2022/Community"
#   cmake --build build --config RelWithDebInfo
# Output: build/bin/OpenWheels/<Config>/OpenWheels.exe (+ runtime DLLs).
# (build/<Config>/OpenWheels.lib/.exp are only the import library the linker
# emits because the prebuilt libchipmunk.lib carries dllexport directives.)

# 3.12+: file(GLOB_RECURSE ... CONFIGURE_DEPENDS)
cmake_minimum_required(VERSION 3.12)

set(APP_NAME OpenWheels)

project(${APP_NAME} C CXX)

set(COCOS2DX_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/cocos2d-x)
set(CMAKE_MODULE_PATH ${COCOS2DX_ROOT_PATH}/cmake/Modules/)

if(NOT EXISTS "${COCOS2DX_ROOT_PATH}/cocos/cocos2d.h")
    message(FATAL_ERROR "cocos2d-x 3.17.2 not found in ${COCOS2DX_ROOT_PATH}; run tools/fetch_engine.ps1 first.")
endif()
if(NOT WIN32)
    message(FATAL_ERROR "This CMakeLists only supports the native Windows build for now.")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "cocos2d-x 3.17.2 only ships 32-bit win32 prebuilt libs: configure with -A Win32.")
endif()

# ---------------------------------------------------------------------------
# Engine module selection (must be set before the engine declares its options)
# ---------------------------------------------------------------------------
# 3.17.2 has no USE_BOX2D cmake switch any more: Box2D is the v3-deps-158
# prebuilt `ext_box2d` (external/Box2D/prebuilt/win32/{debug,release}/libbox2d.lib),
# which is always part of the engine's `external` target. The option is kept
# as a guard so nobody turns it off by accident.
option(USE_BOX2D "Link Box2D (required by the game, must stay ON)" ON)
if(NOT USE_BOX2D)
    message(FATAL_ERROR "OpenWheels requires Box2D: USE_BOX2D must be ON.")
endif()

option(BUILD_LUA_LIBS "Build lua libraries" OFF)
option(BUILD_JS_LIBS "Build js libraries" OFF)
# extensions: cocos2d::extension::TableView/ScrollView (+ assets-manager, Particle3D, physics-nodes)
option(BUILD_EXTENSIONS "Build extension library" ON)
# The reference libMyGame.so references none of cocostudio / cocosbuilder / spine.
option(BUILD_EDITOR_COCOSBUILDER "Build editor support for cocosbuilder" OFF)
option(BUILD_EDITOR_COCOSTUDIO "Build editor support for cocostudio" OFF)
option(BUILD_EDITOR_SPINE "Build editor support for spine" OFF)

include(CocosBuildSet)

# CocosConfigDefine forces CMAKE_CONFIGURATION_TYPES to Debug;Release - add
# RelWithDebInfo back (optimized + PDBs, the day-to-day config). Prebuilt libs
# that only come in debug/release flavors (Box2D, chipmunk, bullet) use their
# release flavor for RelWithDebInfo / MinSizeRel; the trailing empty entry
# falls back to the configuration-less IMPORTED_LOCATION of the other prebuilts.
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;RelWithDebInfo" CACHE STRING "Configurations" FORCE)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO "RelWithDebInfo;Release;")
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL "MinSizeRel;Release;")

# Engine era: C++14 (CocosConfigDefine says 11; MSVC's floor is 14 anyway).
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # Applies to the engine and the game alike (directory scope, set before add_subdirectory).
    #   /MP         parallel compilation inside each project
    #   /utf-8      sources and narrow literals are UTF-8 (what clang does for the Android build)
    #   /fp:precise MSVC default, spelled out: never let anyone slip /fp:fast in
    add_compile_options(/MP /utf-8 /fp:precise)
endif()

add_subdirectory(${COCOS2DX_ROOT_PATH}/cocos ${ENGINE_BINARY_PATH}/cocos/core)

# ---------------------------------------------------------------------------
# Game sources: everything under src/ is picked up automatically (recursively),
# so new files in src/game/** (or anywhere else under src/) need no CMake edits -
# just re-run the build. The only folders left out are other operating systems'
# entry points, src/platform/<os>/; src/platform/win32/ and the cross-platform
# helper folders (src/platform/debug/, src/platform/stubs/, ...) are built.
# ---------------------------------------------------------------------------
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/win32/*.rc
     )
foreach(foreign_os android ios mac linux winrt tizen emscripten)
    list(FILTER GAME_SOURCES EXCLUDE REGEX "/src/platform/${foreign_os}/")
endforeach()


add_executable(${APP_NAME} ${GAME_SOURCES})

# The original's rand()/srand()/RAND_MAX are bionic's (BSD random(), 31-bit); force the same
# generator into the reconstructed game code only (engine code keeps the MSVC CRT's).
if(MSVC)
    set(OW_GAME_TUS ${GAME_SOURCES})
    list(FILTER OW_GAME_TUS INCLUDE REGEX "/src/game/.*\\.cpp$")
    if(OW_GAME_TUS)
        set_source_files_properties(${OW_GAME_TUS} PROPERTIES COMPILE_OPTIONS "/FIplatform/compat/BionicRand.h")
    endif()
endif()

target_link_libraries(${APP_NAME} cocos2d)
# Same roots as tools/check_tu.sh (src, src/game, src/platform/stubs), with the
# AppDelegate directory first (duplicates are dropped, first one wins).
# The reconstruction keeps the original's flat includes (#include "LevelB2D.h"): every
# src/game/<subsystem>/ directory is on the include path.
file(GLOB OW_GAME_DIRS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/src/game/* ${CMAKE_CURRENT_SOURCE_DIR}/src/editor/*)
list(FILTER OW_GAME_DIRS INCLUDE REGEX "/src/(game|editor)/[^/]+$")
target_include_directories(${APP_NAME}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE ${OW_GAME_DIRS}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/stubs
    PRIVATE ${COCOS2DX_ROOT_PATH}/cocos/audio/include/
)

# bin/OpenWheels/<Config>/OpenWheels.exe, /SUBSYSTEM:WINDOWS, IDE source groups
setup_cocos_app_config(${APP_NAME})
if(MSVC)
    # As in the engine's own VS projects (IgnoreSpecificDefaultLibraries): the
    # prebuilt freetype/libjpeg/libwebp were compiled with /MT and request
    # LIBCMT, the release-only /MD prebuilts (libpng, glfw3, libtiff...) request
    # MSVCRT even in Debug. Nothing is resolved from either (every CRT symbol
    # binds to the exe's own DLL CRT, checked with /VERBOSE and dumpbin), so keep
    # them out for good - a second CRT must never sneak in - which also drops the
    # LNK4098 warnings. LNK4099 / LNK4075: the prebuilt libtiff.lib ships without
    # its PDB and was compiled with /ZI.
    set_property(TARGET ${APP_NAME} APPEND_STRING PROPERTY LINK_FLAGS " /NODEFAULTLIB:libcmt.lib /ignore:4099 /ignore:4075")
    set_property(TARGET ${APP_NAME} APPEND_STRING PROPERTY LINK_FLAGS_DEBUG " /NODEFAULTLIB:msvcrt.lib")
endif()
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${APP_NAME})
set_target_properties(${APP_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${APP_NAME}>")

# PC platform layer (keyboard -> virtual touches, --dump-world verification runner). Needs the
# reconstructed game to link, so it stays OFF until src/game is complete.
option(OW_WITH_PC_LAYER "Build the PC keyboard bridge and --dump-world runner" ON)
if(OW_WITH_PC_LAYER)
    target_compile_definitions(${APP_NAME} PRIVATE OW_WITH_PC_LAYER=1)
endif()

# Data tables generated from the PLAYER'S OWN copy of the game (never committed): the sound
# table (by running the original SoundList constructor in an arm64 emulator) and long UI text.
set(OW_GAME_LIB "${CMAKE_CURRENT_SOURCE_DIR}/binary/HappyWheels_Android/config.arm64_v8a/lib/arm64-v8a/libMyGame.so"
    CACHE FILEPATH "Original libMyGame.so (Android 1.1.3 arm64) from your own copy of the game")
find_package(Python3 COMPONENTS Interpreter)
if(TRUE)
    if(Python3_FOUND AND EXISTS "${OW_GAME_LIB}")
        add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/re/extract_soundlist.py
                    --so "${OW_GAME_LIB}" --out "$<TARGET_FILE_DIR:${APP_NAME}>/soundlist.tsv"
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/re/extract_gametext.py
                    --so "${OW_GAME_LIB}" --out "$<TARGET_FILE_DIR:${APP_NAME}>/gametext.tsv"
            COMMENT "Generating soundlist.tsv / gametext.tsv from ${OW_GAME_LIB}")
    else()
        message(WARNING "Python3 or ${OW_GAME_LIB} not found: soundlist.tsv/gametext.tsv will not be "
                        "generated (sounds and long texts will be missing). Set OW_GAME_LIB.")
    endif()
endif()

# Copy the runtime DLLs of every imported engine dependency next to the exe
# (glew32, libcurl, websockets, sqlite3, libmpg123, OpenAL32, libogg/vorbis,
# zlib1, iconv, libcrypto/libssl ...), like the template's post-build step.
cocos_copy_target_dll(${APP_NAME})
