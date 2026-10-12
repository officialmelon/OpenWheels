# OpenWheels' Box2D: the source of cocos2d-x's v3-deps-158 prebuilt libbox2d (erincatto/box2d
# f655c603 + cocos2d-x-3rd-party-libs-src's cocos2d.patch), patched with the browser game's
# Box2D 2.0 solver rules behind the g_flash20Solver switch (README.md).
#
# Included by the root CMakeLists.txt and android/app/CMakeLists.txt right after the engine
# (add_subdirectory(cocos)): it builds `ow_box2d` and puts it in the engine's `external` target in
# place of the prebuilt `ext_box2d`, so the game, the engine and its extensions all link this one.

file(GLOB_RECURSE OW_BOX2D_SOURCES CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/Box2D/*.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/Box2D/*.h)
add_library(ow_box2d STATIC ${OW_BOX2D_SOURCES})
target_include_directories(ow_box2d PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(ow_box2d PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    # Box2D's own float conversion warnings.
    target_compile_options(ow_box2d PRIVATE /wd4244 /wd4305)
else()
    # Every float operation rounded on its own, as in the arm64 prebuilt the original game linked
    # (no fused multiply-adds in external/Box2D/prebuilt/android/arm64-v8a/libbox2d.a).
    target_compile_options(ow_box2d PRIVATE -ffp-contract=off -fno-fast-math)
endif()

if(TARGET external)
    foreach(property LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(ow_external_libs external ${property})
        if(ow_external_libs)
            list(TRANSFORM ow_external_libs REPLACE "^ext_box2d$" "ow_box2d")
            set_property(TARGET external PROPERTY ${property} "${ow_external_libs}")
        endif()
    endforeach()
else()
    message(FATAL_ERROR "thirdparty/box2d must be added after the cocos2d-x engine (target `external`).")
endif()
