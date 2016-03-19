cmake_minimum_required(VERSION 2.8.12 FATAL_ERROR)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
project(darkplaces C)

set(DP_CLIENT 1)
set(DP_SERVER 1)

set(DP_IOS 0)

set(DP_MENU 1)
set(DP_CD 1)
set(DP_VIDEO_CAPTURE 1)

set(USE_ZLIB 1)
set(USE_JPEG 1)

set(SOURCE_COMMON
        builddate.c
        bih.c bih.h
        crypto.c crypto.h
        cl_collision.c cl_collision.h
        cl_demo.c
        cl_dyntexture.c cl_dyntexture.h
        cl_input.c
        cl_main.c
        cl_parse.c
        cl_particles.c
        cl_screen.c cl_screen.h
        cl_video.c cl_video.h
        clvm_cmds.c clvm_cmds.h
        cmd.c cmd.h
        collision.c collision.h
        common.c common.h
        console.c console.h
        csprogs.c csprogs.h
        curves.c curves.h
        cvar.c cvar.h
        dpsoftrast.c dpsoftrast.h
        dpvsimpledecode.c dpvsimpledecode.h
        filematch.c
        fractalnoise.c
        fs.c fs.h
        ft2.c ft2.h ft2_defs.h ft2_fontdefs.h
        utf8lib.c utf8lib.h
        gl_backend.c gl_backend.h
        gl_draw.c lhfont.h
        gl_rmain.c
        gl_rsurf.c
        gl_textures.c
        hmac.c hmac.h
        host.c
        host_cmd.c
        image.c image.h
        image_png.c image_png.h
        jpeg.c jpeg.h
        keys.c keys.h
        lhnet.c lhnet.h
        libcurl.c libcurl.h
        mathlib.c mathlib.h
        matrixlib.c matrixlib.h
        mdfour.c mdfour.h
        meshqueue.c meshqueue.h
        mod_skeletal_animatevertices_sse.c mod_skeletal_animatevertices_sse.h
        mod_skeletal_animatevertices_generic.c mod_skeletal_animatevertices_generic.h
        model_alias.c model_alias.h
        model_brush.c model_brush.h
        model_shared.c model_shared.h
        model_sprite.c model_sprite.h
        netconn.c netconn.h
        palette.c palette.h
        polygon.c polygon.h
        portals.c portals.h
        protocol.c protocol.h
        prvm_cmds.c prvm_cmds.h
        prvm_edict.c
        prvm_exec.c
        r_explosion.c
        r_lerpanim.c r_lerpanim.h
        r_lightning.c
        r_modules.c r_modules.h
        r_shadow.c r_shadow.h
        r_sky.c
        r_sprites.c
        sbar.c sbar.h
        sv_demo.c sv_demo.h
        sv_main.c
        sv_move.c
        sv_phys.c
        sv_user.c
        svbsp.c svbsp.h
        svvm_cmds.c
        sys_shared.c
        vid_shared.c vid.h
        view.c
        wad.c wad.h
        world.c world.h
        zone.c zone.h
        )
set(SOURCE_CLIENT)
set(SOURCE_SERVER)

set(DP_DEFS)
set(DP_DEFS_CLIENT)
set(DP_DEFS_SERVER)

if (DP_MENU)
    list(APPEND DP_DEFS -DCONFIG_MENU)
    list(APPEND SOURCE_COMMON
            menu.c menu.h
            mprogdefs.h
            mvm_cmds.c
            )
endif ()

if (DP_CD)
    list(APPEND DP_DEFS_CLIENT -DCONFIG_CD)
    list(APPEND SOURCE_CLIENT cd_shared.c cdaudio.h)
    list(APPEND SOURCE_CLIENT cd_sdl.c)
endif ()

if (DP_VIDEO_CAPTURE)
    list(APPEND DP_DEFS_CLIENT -DCONFIG_VIDEO_CAPTURE)
    list(APPEND SOURCE_CLIENT
            cap_avi.c cap_avi.h
            cap_ogg.c cap_ogg.h
            )
endif ()

if (WIN32)
    list(APPEND SOURCE_COMMON conproc.c conproc.h)
endif ()

if (DP_IOS)
    list(APPEND DP_DEFS -D__IPHONEOS__)
    set(CMAKE_OSX_SYSROOT iphoneos)
    set(CMAKE_OSX_ARCHITECTURES "$(ARCHS_STANDARD)")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.xonotic\${PRODUCT_NAME:identifier}")
endif ()

list(APPEND SOURCE_CLIENT vid_sdl.c)
list(APPEND SOURCE_SERVER vid_null.c)

list(APPEND SOURCE_CLIENT
        snd_main.c snd_main.h
        snd_mem.c
        snd_mix.c
        snd_ogg.c snd_ogg.h
        snd_wav.c snd_wav.h
        )
list(APPEND SOURCE_CLIENT snd_sdl.c)
list(APPEND SOURCE_SERVER snd_null.c)

list(APPEND SOURCE_COMMON sys.h)
list(APPEND SOURCE_CLIENT sys_sdl.c)
if (WIN32)
    list(APPEND SOURCE_SERVER sys_win.c)
else ()
    list(APPEND SOURCE_SERVER sys_linux.c)
endif ()

list(APPEND SOURCE_COMMON thread.h)
list(APPEND SOURCE_CLIENT thread_sdl.c)
list(APPEND SOURCE_SERVER thread_null.c)

if (DP_SERVER)
    add_executable(darkplaces-dedicated WIN32 MACOSX_BUNDLE ${SOURCE_SERVER} ${SOURCE_COMMON})
endif ()
if (DP_CLIENT)
    add_executable(darkplaces WIN32 MACOSX_BUNDLE ${SOURCE_CLIENT} ${SOURCE_COMMON})
endif ()

if (DP_SERVER)
    target_compile_definitions(darkplaces-dedicated PRIVATE ${DP_DEFS} ${DP_DEFS_SERVER})
endif ()
if (DP_CLIENT)
    target_compile_definitions(darkplaces PRIVATE ${DP_DEFS} ${DP_DEFS_CLIENT})
endif ()

set(LIBS_COMMON ${CMAKE_DL_LIBS})
if (TARGET d0_blind_id)
    list(APPEND LIBS_COMMON d0_blind_id)
    # list(APPEND DP_DEFS -DLINK_TO_CRYPTO)
endif ()
if (UNIX)
    list(APPEND LIBS_COMMON m)
endif ()
if (WIN32)
    list(APPEND LIBS_COMMON ws2_32 winmm)
endif ()
if (APPLE)
    list(APPEND LIBS_COMMON "-framework IOKit")
endif ()

function(target_requires_zlib target)
    if (NOT USE_ZLIB)
        return()
    endif ()
    find_package(ZLIB REQUIRED)
    target_include_directories(${target} PRIVATE ${ZLIB_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE ${ZLIB_LIBRARIES})
    target_compile_definitions(${target} PRIVATE -DLINK_TO_ZLIB)
endfunction()

function(target_requires_jpeg target)
    if (NOT USE_JPEG)
        return()
    endif ()
    find_package(JPEG REQUIRED)
    if (NOT JPEG_INCLUDE_DIRS)
        set(JPEG_INCLUDE_DIRS ${JPEG_INCLUDE_DIR})
    endif ()
    target_include_directories(${target} PRIVATE ${JPEG_INCLUDE_DIRS})
    target_link_libraries(${target} PRIVATE ${JPEG_LIBRARIES})
    target_compile_definitions(${target} PRIVATE -DLINK_TO_LIBJPEG)
endfunction()

if (DP_SERVER)
    target_link_libraries(darkplaces-dedicated PRIVATE ${LIBS_COMMON})
    target_requires_zlib(darkplaces-dedicated)
endif ()

if (DP_CLIENT)
    target_link_libraries(darkplaces PRIVATE ${LIBS_COMMON})
    target_requires_zlib(darkplaces)

    find_package(SDL2 REQUIRED)
    if (NOT SDL2_INCLUDE_DIRS)
        set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
    endif ()
    if (NOT SDL2_LIBRARIES)
        set(SDL2_LIBRARIES ${SDL2_LIBRARY})
    endif ()
    target_include_directories(darkplaces PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(darkplaces PRIVATE ${SDL2_LIBRARIES})

    if (NOT DP_IOS)
        target_requires_jpeg(darkplaces)
    endif ()
endif ()
