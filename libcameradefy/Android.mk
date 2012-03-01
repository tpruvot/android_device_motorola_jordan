LOCAL_PATH := $(call my-dir)

ifeq ($(BOARD_OVERLAY_BASED_CAMERA_HAL),true)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS    := optional
LOCAL_MODULE_PATH    := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_MODULE         := camera.jordan
LOCAL_SRC_FILES      := cameraHal.cpp
LOCAL_PRELINK_MODULE := false

# kept as cm7 reference from (defy) omap3-compat liboverlay
# LOCAL_SRC_FILES += v4l2_utils.c overlay.cpp

LOCAL_C_INCLUDES += $(ANDROID_BUILD_TOP)/frameworks/base/include

LOCAL_SHARED_LIBRARIES += \
    liblog \
    libutils \
    libbinder \
    libcutils \
    libmedia \
    libhardware \
    libcamera_client \
    libdl \
    libui \
    libcamera \

include $(BUILD_SHARED_LIBRARY)

endif # BOARD_OVERLAY_BASED_CAMERA_HAL