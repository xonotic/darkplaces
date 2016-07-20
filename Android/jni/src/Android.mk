LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL
LIBOGG_PATH := ../ogg/libogg-1.3.2
LIBVORBIS_PATH := ../vorbis/libvorbis-1.3.5
DARKPLACES_PATH := ../../..

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/$(SDL_PATH)/include \
	$(LOCAL_PATH)/$(LIBOGG_PATH)/include \
	$(LOCAL_PATH)/$(LIBVORBIS_PATH)/include

LOCAL_CFLAGS := \
	-D_FILE_OFFSET_BITS=64 \
	-D__KERNEL_STRICT_NAMES \
	-DCONFIG_MENU \
	-DCONFIG_VIDEO_CAPTURE

# Add your application source files here...
# Note: this is the expansion of $(OBJ_CD) in Darkplaces's own Makefile.
LOCAL_SRC_FILES := $(SDL_PATH)/src/main/android/SDL_android_main.c \
	\
	$(DARKPLACES_PATH)/builddate.c \
	$(DARKPLACES_PATH)/sys_sdl.c \
	$(DARKPLACES_PATH)/vid_sdl.c \
	$(DARKPLACES_PATH)/thread_sdl.c \
	\
	$(DARKPLACES_PATH)/menu.c \
	$(DARKPLACES_PATH)/mvm_cmds.c \
	\
	$(DARKPLACES_PATH)/snd_main.c \
	$(DARKPLACES_PATH)/snd_mem.c \
	$(DARKPLACES_PATH)/snd_mix.c \
	$(DARKPLACES_PATH)/snd_ogg.c \
	$(DARKPLACES_PATH)/snd_wav.c \
	\
	$(DARKPLACES_PATH)/snd_sdl.c \
	\
	$(DARKPLACES_PATH)/cd_shared.c \
	$(DARKPLACES_PATH)/cd_null.c \
	\
	$(DARKPLACES_PATH)/cap_avi.c \
	$(DARKPLACES_PATH)/cap_ogg.c \
	\
	$(DARKPLACES_PATH)/bih.c \
	$(DARKPLACES_PATH)/crypto.c \
	$(DARKPLACES_PATH)/cl_collision.c \
	$(DARKPLACES_PATH)/cl_demo.c \
	$(DARKPLACES_PATH)/cl_dyntexture.c \
	$(DARKPLACES_PATH)/cl_input.c \
	$(DARKPLACES_PATH)/cl_main.c \
	$(DARKPLACES_PATH)/cl_parse.c \
	$(DARKPLACES_PATH)/cl_particles.c \
	$(DARKPLACES_PATH)/cl_screen.c \
	$(DARKPLACES_PATH)/cl_video.c \
	$(DARKPLACES_PATH)/clvm_cmds.c \
	$(DARKPLACES_PATH)/cmd.c \
	$(DARKPLACES_PATH)/collision.c \
	$(DARKPLACES_PATH)/common.c \
	$(DARKPLACES_PATH)/console.c \
	$(DARKPLACES_PATH)/csprogs.c \
	$(DARKPLACES_PATH)/curves.c \
	$(DARKPLACES_PATH)/cvar.c \
	$(DARKPLACES_PATH)/dpsoftrast.c \
	$(DARKPLACES_PATH)/dpvsimpledecode.c \
	$(DARKPLACES_PATH)/filematch.c \
	$(DARKPLACES_PATH)/fractalnoise.c \
	$(DARKPLACES_PATH)/fs.c \
	$(DARKPLACES_PATH)/ft2.c \
	$(DARKPLACES_PATH)/utf8lib.c \
	$(DARKPLACES_PATH)/gl_backend.c \
	$(DARKPLACES_PATH)/gl_draw.c \
	$(DARKPLACES_PATH)/gl_rmain.c \
	$(DARKPLACES_PATH)/gl_rsurf.c \
	$(DARKPLACES_PATH)/gl_textures.c \
	$(DARKPLACES_PATH)/hmac.c \
	$(DARKPLACES_PATH)/host.c \
	$(DARKPLACES_PATH)/host_cmd.c \
	$(DARKPLACES_PATH)/image.c \
	$(DARKPLACES_PATH)/image_png.c \
	$(DARKPLACES_PATH)/jpeg.c \
	$(DARKPLACES_PATH)/keys.c \
	$(DARKPLACES_PATH)/lhnet.c \
	$(DARKPLACES_PATH)/libcurl.c \
	$(DARKPLACES_PATH)/mathlib.c \
	$(DARKPLACES_PATH)/matrixlib.c \
	$(DARKPLACES_PATH)/mdfour.c \
	$(DARKPLACES_PATH)/meshqueue.c \
	$(DARKPLACES_PATH)/mod_skeletal_animatevertices_sse.c \
	$(DARKPLACES_PATH)/mod_skeletal_animatevertices_generic.c \
	$(DARKPLACES_PATH)/model_alias.c \
	$(DARKPLACES_PATH)/model_brush.c \
	$(DARKPLACES_PATH)/model_shared.c \
	$(DARKPLACES_PATH)/model_sprite.c \
	$(DARKPLACES_PATH)/netconn.c \
	$(DARKPLACES_PATH)/palette.c \
	$(DARKPLACES_PATH)/polygon.c \
	$(DARKPLACES_PATH)/portals.c \
	$(DARKPLACES_PATH)/protocol.c \
	$(DARKPLACES_PATH)/prvm_cmds.c \
	$(DARKPLACES_PATH)/prvm_edict.c \
	$(DARKPLACES_PATH)/prvm_exec.c \
	$(DARKPLACES_PATH)/r_explosion.c \
	$(DARKPLACES_PATH)/r_lerpanim.c \
	$(DARKPLACES_PATH)/r_lightning.c \
	$(DARKPLACES_PATH)/r_modules.c \
	$(DARKPLACES_PATH)/r_shadow.c \
	$(DARKPLACES_PATH)/r_sky.c \
	$(DARKPLACES_PATH)/r_sprites.c \
	$(DARKPLACES_PATH)/sbar.c \
	$(DARKPLACES_PATH)/sv_demo.c \
	$(DARKPLACES_PATH)/sv_main.c \
	$(DARKPLACES_PATH)/sv_move.c \
	$(DARKPLACES_PATH)/sv_phys.c \
	$(DARKPLACES_PATH)/sv_user.c \
	$(DARKPLACES_PATH)/svbsp.c \
	$(DARKPLACES_PATH)/svvm_cmds.c \
	$(DARKPLACES_PATH)/sys_shared.c \
	$(DARKPLACES_PATH)/vid_shared.c \
	$(DARKPLACES_PATH)/view.c \
	$(DARKPLACES_PATH)/wad.c \
	$(DARKPLACES_PATH)/world.c \
	$(DARKPLACES_PATH)/zone.c

LOCAL_SHARED_LIBRARIES := \
	SDL2 \
	ogg \
	vorbis

LOCAL_LDLIBS := \
	-lGLESv1_CM -lGLESv2 -llog -lz

include $(BUILD_SHARED_LIBRARY)
