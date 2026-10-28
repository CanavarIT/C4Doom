cmake_minimum_required(VERSION 3.22.1)
project(c4doom LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# android_native_app_glue is plain C shipped with the NDK
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)

add_library(c4doom SHARED
    ${APP_GLUE_DIR}/android_native_app_glue.c
    platform_android.c
    view.c
    game.c
    ai.c
    phys.c
    specials.c
    tables.c
    level.c
    render.c
    canvas.c
    hud.c
    menu.c
    intermission.c
    savegame.c
    sound.c
    music.c
    prefs.c
    gfx.c
    wad.c)

target_include_directories(c4doom PRIVATE ${APP_GLUE_DIR})

# -fwrapv: Doom style fixed point / hash arithmetic relies on wrap-around
target_compile_options(c4doom PRIVATE -O2 -fwrapv -fno-strict-aliasing -Wall -Wno-unused-parameter -Wno-deprecated-declarations)

# keep ANativeActivity_onCreate from the static glue library
set_target_properties(c4doom PROPERTIES LINK_FLAGS "-u ANativeActivity_onCreate")

target_link_libraries(c4doom android log OpenSLES m)
