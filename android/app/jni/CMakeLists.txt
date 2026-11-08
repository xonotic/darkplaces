# DarkPlaces / Xonotic Android native build.
# Builds libSDL2.so and libmain.so (the engine) plus static third-party deps.
cmake_minimum_required(VERSION 3.18)
project(darkplaces_android C CXX)

include(FetchContent)
set(FETCHCONTENT_QUIET ON)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# ---------------------------------------------------------------- SDL2
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(sdl2
	URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz)
FetchContent_MakeAvailable(sdl2)

# ---------------------------------------------------------------- libjpeg-turbo (sources compiled directly)
FetchContent_Declare(jpeg
	URL https://github.com/libjpeg-turbo/libjpeg-turbo/archive/refs/tags/2.1.5.1.tar.gz)
FetchContent_GetProperties(jpeg)
if(NOT jpeg_POPULATED)
	FetchContent_Populate(jpeg)
endif()
set(JPEG_SRC jcapimin.c jcapistd.c jccoefct.c jccolor.c jcdctmgr.c jchuff.c jcicc.c jcinit.c
	jcmainct.c jcmarker.c jcmaster.c jcomapi.c jcparam.c jcphuff.c jcprepct.c jcsample.c
	jctrans.c jdapimin.c jdapistd.c jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c jddctmgr.c
	jdhuff.c jdicc.c jdinput.c jdmainct.c jdmarker.c jdmaster.c jdmerge.c jdphuff.c jdpostct.c
	jdsample.c jdtrans.c jerror.c jfdctflt.c jfdctfst.c jfdctint.c jidctflt.c jidctfst.c
	jidctint.c jidctred.c jquant1.c jquant2.c jutils.c jmemmgr.c jmemnobs.c jsimd_none.c
	jaricom.c jcarith.c jdarith.c)
list(TRANSFORM JPEG_SRC PREPEND ${jpeg_SOURCE_DIR}/)
set(JPEG_GEN ${CMAKE_CURRENT_BINARY_DIR}/jpeg_gen)
file(WRITE ${JPEG_GEN}/jconfig.h
"#define JPEG_LIB_VERSION 62\n#define LIBJPEG_TURBO_VERSION 2.1.5.1\n#define LIBJPEG_TURBO_VERSION_NUMBER 2001005\n#define C_ARITH_CODING_SUPPORTED 1\n#define D_ARITH_CODING_SUPPORTED 1\n#define BITS_IN_JSAMPLE 8\n")
file(WRITE ${JPEG_GEN}/jconfigint.h
"#define BUILD \"android\"\n#define HIDDEN __attribute__((visibility(\"hidden\")))\n#define INLINE inline __attribute__((always_inline))\n#define THREAD_LOCAL __thread\n#define PACKAGE_NAME \"libjpeg-turbo\"\n#define VERSION \"2.1.5.1\"\n#if __SIZEOF_SIZE_T__ == 8\n#define SIZEOF_SIZE_T 8\n#else\n#define SIZEOF_SIZE_T 4\n#endif\n#define HAVE_BUILTIN_CTZL\n#define FALLTHROUGH __attribute__((fallthrough));\n#define C_ARITH_CODING_SUPPORTED 1\n#define D_ARITH_CODING_SUPPORTED 1\n#define WITH_SIMD 0\n")
file(WRITE ${JPEG_GEN}/jversion.h
"#define JVERSION \"6b  27-Mar-1998\"\n#define JCOPYRIGHT \"libjpeg-turbo\"\n#define JCOPYRIGHT_SHORT \"libjpeg-turbo\"\n")
add_library(jpeg_static STATIC ${JPEG_SRC})
target_include_directories(jpeg_static PUBLIC ${JPEG_GEN} ${jpeg_SOURCE_DIR})
target_compile_options(jpeg_static PRIVATE -w)

# ---------------------------------------------------------------- libogg / libvorbis
FetchContent_Declare(ogg URL https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.gz)
FetchContent_Declare(vorbis URL https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.gz)
FetchContent_GetProperties(ogg)
if(NOT ogg_POPULATED)
	FetchContent_Populate(ogg)
endif()
FetchContent_GetProperties(vorbis)
if(NOT vorbis_POPULATED)
	FetchContent_Populate(vorbis)
endif()
set(OGG_GEN ${CMAKE_CURRENT_BINARY_DIR}/ogg_gen)
file(WRITE ${OGG_GEN}/ogg/config_types.h
"#ifndef __CONFIG_TYPES_H__\n#define __CONFIG_TYPES_H__\n#include <stdint.h>\ntypedef int16_t ogg_int16_t;\ntypedef uint16_t ogg_uint16_t;\ntypedef int32_t ogg_int32_t;\ntypedef uint32_t ogg_uint32_t;\ntypedef int64_t ogg_int64_t;\ntypedef uint64_t ogg_uint64_t;\n#endif\n")
add_library(ogg_static STATIC ${ogg_SOURCE_DIR}/src/bitwise.c ${ogg_SOURCE_DIR}/src/framing.c)
target_include_directories(ogg_static PUBLIC ${OGG_GEN} ${ogg_SOURCE_DIR}/include)
target_compile_options(ogg_static PRIVATE -w)

set(VORBIS_SRC mdct.c smallft.c block.c envelope.c window.c lsp.c lpc.c analysis.c synthesis.c
	psy.c info.c floor1.c floor0.c res0.c mapping0.c registry.c codebook.c sharedbook.c
	lookup.c bitrate.c vorbisfile.c)
list(TRANSFORM VORBIS_SRC PREPEND ${vorbis_SOURCE_DIR}/lib/)
add_library(vorbis_static STATIC ${VORBIS_SRC})
target_include_directories(vorbis_static PUBLIC ${vorbis_SOURCE_DIR}/include PRIVATE ${vorbis_SOURCE_DIR}/lib)
target_link_libraries(vorbis_static PUBLIC ogg_static)
target_compile_options(vorbis_static PRIVATE -w)

# ---------------------------------------------------------------- FreeType
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "" FORCE)
FetchContent_Declare(freetype
	URL https://github.com/freetype/freetype/archive/refs/tags/VER-2-13-3.tar.gz)
FetchContent_MakeAvailable(freetype)

# ---------------------------------------------------------------- Engine
set(DP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
set(DP_SRC
	builddate.c cap_avi.c cap_ogg.c sys_sdl.c vid_sdl.c thread_sdl.c menu.c mvm_cmds.c
	snd_main.c snd_mem.c snd_mix.c snd_ogg.c snd_wav.c snd_sdl.c
	bih.c crypto.c cd_shared.c cl_cmd.c cl_collision.c cl_demo.c cl_ents.c cl_ents4.c cl_ents5.c
	cl_ents_nq.c cl_ents_qw.c cl_input.c cl_main.c cl_parse.c cl_particles.c cl_screen.c
	cl_video.c cl_video_libavw.c clvm_cmds.c cmd.c collision.c com_crc16.c com_ents.c com_ents4.c
	com_game.c com_infostring.c com_msg.c common.c console.c csprogs.c curves.c cvar.c
	dpvsimpledecode.c filematch.c fractalnoise.c fs.c ft2.c utf8lib.c gl_backend.c gl_draw.c
	gl_rmain.c gl_rsurf.c gl_textures.c hmac.c host.c image.c image_png.c jpeg.c keys.c lhnet.c
	libcurl.c mathlib.c matrixlib.c mdfour.c meshqueue.c mod_skeletal_animatevertices_sse.c
	mod_skeletal_animatevertices_generic.c model_alias.c model_brush.c model_shared.c
	model_sprite.c netconn.c palette.c phys.c polygon.c portals.c protocol.c prvm_cmds.c
	prvm_edict.c prvm_exec.c r_explosion.c r_lightning.c r_modules.c r_shadow.c r_sky.c
	r_sprites.c r_stats.c sbar.c sv_ccmds.c sv_demo.c sv_ents.c sv_ents4.c sv_ents5.c
	sv_ents_csqc.c sv_ents_nq.c sv_main.c sv_move.c sv_phys.c sv_save.c sv_send.c sv_user.c
	svbsp.c svvm_cmds.c sys_shared.c taskqueue.c vid_shared.c view.c wad.c world.c zone.c)
list(TRANSFORM DP_SRC PREPEND ${DP_DIR}/)

# SDLActivity loads "libmain.so" and calls SDL_main.
add_library(main SHARED ${DP_SRC} gles_compat.c android_touchpad.c)
target_include_directories(main PRIVATE ${DP_DIR})
target_compile_definitions(main PRIVATE
	CONFIG_MENU CONFIG_VIDEO_CAPTURE LINK_TO_LIBJPEG BUILDTYPE=release _FILE_OFFSET_BITS=64 SDL_DISABLE_IMMINTRIN_H)
target_compile_options(main PRIVATE -O3 -fno-strict-aliasing -Wno-everything)
target_link_libraries(main PRIVATE SDL2::SDL2 jpeg_static vorbis_static ogg_static freetype
	z GLESv3 log android m dl)
