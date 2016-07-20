LOCAL_PATH := $(call my-dir)

###########################
#
# libvorbis shared library
#
###########################

include $(CLEAR_VARS)

LOCAL_MODULE := vorbis

LIBOGG_PATH := ../ogg/libogg-1.3.2

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/libvorbis-1.3.5/include \
	$(LOCAL_PATH)/libvorbis-1.3.5/libs \
	$(LOCAL_PATH)/$(LIBOGG_PATH)/include

LOCAL_EXPORT_C_INCLUDES := \
	$(LOCAL_PATH)/include \

LOCAL_SRC_FILES := \
	libvorbis-1.3.5/lib/mdct.c \
	libvorbis-1.3.5/lib/smallft.c \
	libvorbis-1.3.5/lib/block.c \
	libvorbis-1.3.5/lib/envelope.c \
	libvorbis-1.3.5/lib/window.c \
	libvorbis-1.3.5/lib/lsp.c \
	libvorbis-1.3.5/lib/lpc.c \
	libvorbis-1.3.5/lib/analysis.c \
	libvorbis-1.3.5/lib/synthesis.c \
	libvorbis-1.3.5/lib/psy.c \
	libvorbis-1.3.5/lib/info.c \
	libvorbis-1.3.5/lib/floor1.c \
	libvorbis-1.3.5/lib/floor0.c \
	libvorbis-1.3.5/lib/res0.c \
	libvorbis-1.3.5/lib/mapping0.c \
	libvorbis-1.3.5/lib/registry.c \
	libvorbis-1.3.5/lib/codebook.c \
	libvorbis-1.3.5/lib/sharedbook.c \
	libvorbis-1.3.5/lib/lookup.c \
	libvorbis-1.3.5/lib/bitrate.c \
	libvorbis-1.3.5/lib/vorbisfile.c

LOCAL_SHARED_LIBRARIES := \
	ogg

include $(BUILD_SHARED_LIBRARY)
