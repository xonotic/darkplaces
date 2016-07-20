LOCAL_PATH := $(call my-dir)

###########################
#
# libogg shared library
#
###########################

include $(CLEAR_VARS)

LOCAL_MODULE := ogg

LOCAL_C_INCLUDES := $(LOCAL_PATH)/libogg-1.3.2/include

LOCAL_EXPORT_C_INCLUDES := $(LOCAL_C_INCLUDES)

LOCAL_SRC_FILES := \
	$(subst $(LOCAL_PATH)/,, \
	$(wildcard $(LOCAL_PATH)/libogg-1.3.2/src/*.c))

include $(BUILD_SHARED_LIBRARY)
