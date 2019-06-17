LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES:= early_init.c
LOCAL_MODULE := init_early
LOCAL_STATIC_LIBRARIES := libc++_static
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT)
LOCAL_POST_INSTALL_CMD := $(hide) mkdir -p $(LOCAL_MODULE_PATH)/early_services;
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := early_init.conf
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS = ETC
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/earlyrootfs/etc
#Prepare Early Rootfs structure
LOCAL_POST_INSTALL_CMD := $(hide) mkdir -p $(LOCAL_MODULE_PATH)/../sbin; \
			mkdir -p $(LOCAL_MODULE_PATH)/../system/bin; \
			mkdir -p $(LOCAL_MODULE_PATH)/../system/lib64; \
			mkdir -p $(LOCAL_MODULE_PATH)/../run/early; \
			mkdir -p $(LOCAL_MODULE_PATH)/../proc; \
			mkdir -p $(LOCAL_MODULE_PATH)/../sys; \
			mkdir -p $(LOCAL_MODULE_PATH)/../dev; \
			mkdir -p $(LOCAL_MODULE_PATH)/../etc; \
			mkdir -p $(LOCAL_MODULE_PATH)/../usr/sbin; \
			mkdir -p $(LOCAL_MODULE_PATH)/../vendor/firmware_mnt; \
			mkdir -p $(LOCAL_MODULE_PATH)/../vendor/lib/modules; \
			ln -sf system/bin $(LOCAL_MODULE_PATH)/../bin; \
			ln -sf system/lib64 $(LOCAL_MODULE_PATH)/../lib64; \

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := early_init_eth.conf
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS = ETC
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/earlyrootfs/etc
include $(BUILD_PREBUILT)

