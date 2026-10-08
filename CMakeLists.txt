# OpenWheels - native Windows (Win32 / x86), Linux (x86_64), macOS and iOS builds on top of
# cocos2d-x 3.17.2. (Android has its own CMakeLists in android/app/.)
#
# Linux / macOS:  tools/fetch_engine.sh && tools/build.sh   (see docs/DESKTOP.md)
# iOS:            tools/build.sh --ios   (generates an Xcode project; see docs/IOS.md)
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
cmake_minimum_required(VERSION 3.13)

set(APP_NAME OpenWheels)

project(${APP_NAME} C CXX)

# Debug info embedded in each object (/Z7) instead of one shared compile PDB per target (/Zi):
# with the editor sources the shared vc143.pdb fails mid-build with C3130 ("failed to write
# injected code block to PDB"). The linker still writes OpenWheels.pdb.
if(MSVC)
    foreach(lang C CXX)
        foreach(cfg DEBUG RELWITHDEBINFO)
            string(REGEX REPLACE "/Z[iI]" "/Z7" CMAKE_${lang}_FLAGS_${cfg} "${CMAKE_${lang}_FLAGS_${cfg}}")
        endforeach()
    endforeach()
endif()

set(COCOS2DX_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/cocos2d-x)
set(CMAKE_MODULE_PATH ${COCOS2DX_ROOT_PATH}/cmake/Modules/)

if(NOT EXISTS "${COCOS2DX_ROOT_PATH}/cocos/cocos2d.h")
    message(FATAL_ERROR "cocos2d-x 3.17.2 not found in ${COCOS2DX_ROOT_PATH}; run tools/fetch_engine.ps1 first.")
endif()
if(ANDROID)
    message(FATAL_ERROR "Android builds use android/app/CMakeLists.txt (tools/build_android.ps1).")
endif()
if(WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
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
#
# Platform folders: src/platform/<os>/ holds one OS's entry point; src/platform/desktop/ is shared
# by the PC builds (Windows, Linux, macOS: keyboard bridge, --dump-world runner) and
# src/platform/unix/ by Linux and macOS (POSIX main, crash handler).
if(WIN32)
    set(OW_PLATFORM_DIRS win32 desktop)
elseif(IOS)
    set(OW_PLATFORM_DIRS ios)
elseif(APPLE)
    set(OW_PLATFORM_DIRS mac desktop unix)
else()
    set(OW_PLATFORM_DIRS linux desktop unix)
endif()
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
     ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/win32/*.rc
     ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/ios/*.mm
     ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/ios/*.m
     ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/mac/*.mm
     )
foreach(platform_dir android ios mac linux win32 desktop unix winrt tizen emscripten)
    list(FIND OW_PLATFORM_DIRS ${platform_dir} ow_platform_index)
    if(ow_platform_index EQUAL -1)
        list(FILTER GAME_SOURCES EXCLUDE REGEX "/src/platform/${platform_dir}/")
    endif()
endforeach()

# OW_WITH_EDITOR=OFF leaves out src/editor/ (only a few LevelSession hooks reach it); used by
# private verification build trees, e.g. tools/build.ps1 -BuildDir build_parity.
option(OW_WITH_EDITOR "Build the level editor sources under src/editor/" ON)
if(NOT OW_WITH_EDITOR)
    list(FILTER GAME_SOURCES EXCLUDE REGEX "/src/editor/")
    # NET (PC addition): the LAN level transfer saves into the editor's LevelStore
    list(FILTER GAME_SOURCES EXCLUDE REGEX "/src/net/")
    # the menus' editor hooks reference a few LevelSession members: verification-only stubs
    list(APPEND GAME_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/tools/parity/EditorlessStubs.cpp)
endif()


if(IOS)
    # The iOS launch screen and Info.plist (src/platform/ios/).
    set(OW_IOS_RES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/ios/LaunchScreen.storyboard
        )
    list(APPEND GAME_SOURCES ${OW_IOS_RES})
    # The player's own Android assets, bundled as OpenWheels.app/assets/ when present (otherwise
    # copy them into the app's Documents/assets/ with Finder; see docs/IOS.md).
    set(OW_ANDROID_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/binary/HappyWheels_Android/HW_Android/assets"
        CACHE PATH "The Android game's assets/ folder from your own copy")
    if(EXISTS "${OW_ANDROID_ASSETS}/shared")
        file(GLOB_RECURSE OW_IOS_ASSET_FILES "${OW_ANDROID_ASSETS}/*")
        foreach(asset ${OW_IOS_ASSET_FILES})
            file(RELATIVE_PATH asset_rel "${OW_ANDROID_ASSETS}" "${asset}")
            get_filename_component(asset_dir "${asset_rel}" DIRECTORY)
            set_source_files_properties("${asset}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/assets/${asset_dir}")
        endforeach()
        list(APPEND GAME_SOURCES ${OW_IOS_ASSET_FILES})
    else()
        message(WARNING "No Android assets at ${OW_ANDROID_ASSETS}: the iOS app will ask for them at start-up "
                        "(copy them into its Documents/assets/ with Finder).")
    endif()
endif()

add_executable(${APP_NAME} ${GAME_SOURCES})

# The original's rand()/srand()/RAND_MAX are bionic's (BSD random(), 31-bit); force the same
# generator into the reconstructed game code only (engine code keeps the platform libc's).
set(OW_GAME_TUS ${GAME_SOURCES})
list(FILTER OW_GAME_TUS INCLUDE REGEX "/src/game/.*\\.cpp$")
if(OW_GAME_TUS)
    if(MSVC)
        set_source_files_properties(${OW_GAME_TUS} PROPERTIES COMPILE_OPTIONS "/FIplatform/compat/BionicRand.h")
    else()
        set_source_files_properties(${OW_GAME_TUS} PROPERTIES COMPILE_OPTIONS
            "-include;${CMAKE_CURRENT_SOURCE_DIR}/src/platform/compat/BionicRand.h")
    endif()
endif()
if(LINUX)
    # The v3-deps-158 linux prebuilts (freetype, ...) are not position independent: link a classic
    # (non-PIE) executable, as toolchains of that era did by default.
    target_link_options(${APP_NAME} PRIVATE -no-pie)
endif()
if(NOT MSVC)
    # The reconstruction mirrors the original's (clang, arm64) float behaviour: no FMA contraction
    # (x86_64 GCC would otherwise fuse a*b+c on -march levels with FMA) and no fast-math.
    target_compile_options(${APP_NAME} PRIVATE -ffp-contract=off -fno-fast-math
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-declarations>)
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
if(APPLE)
    find_library(OW_SECURITY_FRAMEWORK Security)
    target_link_libraries(${APP_NAME} ${OW_SECURITY_FRAMEWORK})
    if(IOS)
        set_target_properties(${APP_NAME} PROPERTIES RESOURCE "${OW_IOS_RES}")
        set_target_properties(${APP_NAME} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/ios/Info.plist")
        set_xcode_property(${APP_NAME} IPHONEOS_DEPLOYMENT_TARGET "11.0")
        set_xcode_property(${APP_NAME} TARGETED_DEVICE_FAMILY "1,2")
        set_xcode_property(${APP_NAME} DEVELOPMENT_TEAM "$ENV{OW_IOS_TEAM}")
        set_xcode_property(${APP_NAME} CODE_SIGN_IDENTITY "iPhone Developer")
        set_xcode_property(${APP_NAME} PRODUCT_BUNDLE_IDENTIFIER "org.openwheels.game")
    else()
        set_target_properties(${APP_NAME} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/mac/Info.plist")
    endif()
endif()
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
if(OW_WITH_PC_LAYER AND NOT IOS)
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

# QOL (PC addition): gore sprites for Irresponsible Dad's kid, which the mobile port shipped
# without. Rebuilt from the player's own browser-game SWF with JPEXS FFDec + Java
# (tools/assets/extract_kid_gore.py) into <exe dir>/generated/<tier>/characters/. Skipped quietly
# (the kid stays gore-free) when the SWF, FFDec, Java or Python's numpy/Pillow are missing; the
# script keeps a stamp so unchanged inputs cost nothing on later builds.
set(OW_FLASH_SWF "${CMAKE_CURRENT_SOURCE_DIR}/binary/flash/swf/game_e_v1_87_g.dec.swf"
    CACHE FILEPATH "Decrypted Happy Wheels browser-game SWF (v1.87) from your own copy")
set(OW_FFDEC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/binary/flash/tools/ffdec"
    CACHE PATH "JPEXS FFDec folder (lib/ffdec_lib.jar)")
set(OW_ANDROID_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/binary/HappyWheels_Android/HW_Android/assets"
    CACHE PATH "The Android game's assets/ folder from your own copy")
if(Python3_FOUND)
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/assets/extract_kid_gore.py
                --optional --swf "${OW_FLASH_SWF}" --ffdec "${OW_FFDEC_DIR}"
                --assets "${OW_ANDROID_ASSETS}" --out "$<TARGET_FILE_DIR:${APP_NAME}>/generated"
        COMMENT "Building the kid's gore sprites from ${OW_FLASH_SWF} (skipped if missing)")
endif()

# RESTORED (PC addition): the browser game's player characters the mobile port left out (Lawnmower
# Man so far), rebuilt from the player's own browser character<N>.swf files with FFDec + Java
# (+ ffmpeg for sounds) by tools/assets/extract_character.py into <exe dir>/generated/restored/.
# Skipped quietly (the character list stays the original six) when the inputs are missing.
set(OW_FLASH_CHARACTERS "${CMAKE_CURRENT_SOURCE_DIR}/binary/flash/swf/characters"
    CACHE PATH "Folder with the browser game's character<N>.swf (from your own copy)")
set(OW_FLASH_SOUNDS "${CMAKE_CURRENT_SOURCE_DIR}/binary/flash/swf/happy_sounds_v1_72.swf"
    CACHE FILEPATH "The browser game's sound SWF (from your own copy)")
if(Python3_FOUND)
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/assets/extract_character.py
                --optional --swf-dir "${OW_FLASH_CHARACTERS}" --sounds-swf "${OW_FLASH_SOUNDS}"
                --ffdec "${OW_FFDEC_DIR}" --out "$<TARGET_FILE_DIR:${APP_NAME}>/generated/restored"
        COMMENT "Building the restored browser characters from ${OW_FLASH_CHARACTERS} (skipped if missing)")
endif()

# RESTORED (PC addition): OpenWheels' own campaign chapters for the restored characters
# (res/levels/restored/: chapters.plist and browser-format level XML - our own content, in the
# repository). The game reads them as levels/restored/... through a search path after the
# player's assets: the exe folder on Windows / Linux / macOS (Contents/MacOS inside the .app,
# where generated/ goes too), the bundle root on iOS (bundle resources). Android stages them in
# android/app/build.gradle.
set(OW_CAMPAIGN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res/levels/restored")
if(EXISTS "${OW_CAMPAIGN_DIR}")
    if(IOS)
        file(GLOB_RECURSE OW_CAMPAIGN_FILES CONFIGURE_DEPENDS "${OW_CAMPAIGN_DIR}/*.plist" "${OW_CAMPAIGN_DIR}/*.xml")
        foreach(ow_campaign_file ${OW_CAMPAIGN_FILES})
            file(RELATIVE_PATH ow_campaign_rel "${CMAKE_CURRENT_SOURCE_DIR}/res" "${ow_campaign_file}")
            get_filename_component(ow_campaign_rel_dir "${ow_campaign_rel}" DIRECTORY)
            set_source_files_properties("${ow_campaign_file}" PROPERTIES
                MACOSX_PACKAGE_LOCATION "Resources/${ow_campaign_rel_dir}" HEADER_FILE_ONLY TRUE)
        endforeach()
        target_sources(${APP_NAME} PRIVATE ${OW_CAMPAIGN_FILES})
    else()
        add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${OW_CAMPAIGN_DIR}"
                    "$<TARGET_FILE_DIR:${APP_NAME}>/levels/restored"
            COMMENT "Copying the restored characters' campaign levels")
    endif()
endif()

# ONLINE (PC addition): art for the browser-level items the mobile game lacks (furniture, food,
# glass, buildings, NPCs, text-box fonts...), rendered from the player's own browser-game SWF by
# tools/assets/extract_flash_items.py into <exe dir>/generated/. Without it those items draw as
# plain shapes. Skipped quietly when the inputs are missing.
if(Python3_FOUND)
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/assets/extract_flash_items.py
                --optional --swf "${OW_FLASH_SWF}" --ffdec "${OW_FFDEC_DIR}"
                --out "$<TARGET_FILE_DIR:${APP_NAME}>/generated"
        COMMENT "Building the browser-level item art from ${OW_FLASH_SWF} (skipped if missing)")
endif()

# ONLINE (PC addition): browser-game sounds the Android build lacks (Santa, elves, Helicopter Man,
# the kids, BoomboxHit...), exported from the player's own sound SWF with FFDec + ffmpeg by
# tools/assets/extract_flash_sounds.py into <exe dir>/generated/flash/sounds/ (used by browser
# levels only). Runs after extract_character.py, whose sounds it doesn't duplicate.
if(Python3_FOUND)
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/assets/extract_flash_sounds.py
                --optional --sounds-swf "${OW_FLASH_SOUNDS}" --ffdec "${OW_FFDEC_DIR}"
                --assets "${OW_ANDROID_ASSETS}"
                --skip-dir "$<TARGET_FILE_DIR:${APP_NAME}>/generated/restored/sounds"
                --out "$<TARGET_FILE_DIR:${APP_NAME}>/generated"
        COMMENT "Exporting the browser-level sounds from ${OW_FLASH_SOUNDS} (skipped if missing)")
endif()

# Copy the runtime DLLs of every imported engine dependency next to the exe
# (glew32, libcurl, websockets, sqlite3, libmpg123, OpenAL32, libogg/vorbis,
# zlib1, iconv, libcrypto/libssl ...), like the template's post-build step.
if(WIN32)
    cocos_copy_target_dll(${APP_NAME})
endif()
