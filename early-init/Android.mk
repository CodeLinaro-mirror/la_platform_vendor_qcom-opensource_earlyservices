LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES:= early_init.cpp \
                  util.cpp \
                  log.cpp

LOCAL_MODULE := init_early
LOCAL_STATIC_LIBRARIES := libc++_static
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/early_services
LOCAL_POST_INSTALL_CMD := $(hide) mkdir -p $(TARGET_ROOT_OUT)/early_services;
ifneq ($(BOARD_BUILD_SYSTEM_ROOT_IMAGE),true)
  LOCAL_POST_INSTALL_CMD += mkdir -p $(TARGET_RAMDISK_OUT)/early_services;
endif
LOCAL_STATIC_LIBRARIES := \
     libbase \
     libseccomp_policy \
     libselinux \
     liblog \
     libcrypto_utils \
     libcrypto \

LOCAL_CPPFLAGS := -std=c++17
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
#To be removed later
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := init_early_test
LOCAL_SRC_FILES := test.c
LOCAL_MODULE_CLASS = ETC
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/early_services/system/bin
ifneq ($(BOARD_BUILD_SYSTEM_ROOT_IMAGE),true)
  LOCAL_FORCE_STATIC_EXECUTABLE := true
endif
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := early_init.conf
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS = ETC
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/early_services/etc
#Prepare Early Rootfs structure
LOCAL_POST_INSTALL_CMD := $(hide) mkdir -p $(LOCAL_MODULE_PATH)/../sbin; \
			mkdir -p $(LOCAL_MODULE_PATH)/../system/bin; \
			mkdir -p $(LOCAL_MODULE_PATH)/../system/lib64; \
                        mkdir -p $(LOCAL_MODULE_PATH)/../system/etc; \
                        mkdir -p $(LOCAL_MODULE_PATH)/../vendor/lib64; \
                        mkdir -p $(LOCAL_MODULE_PATH)/../vendor/lib; \
                        mkdir -p $(LOCAL_MODULE_PATH)/../system/etc/selinux; \
			mkdir -p $(LOCAL_MODULE_PATH)/../run/early; \
			mkdir -p $(LOCAL_MODULE_PATH)/../proc; \
			mkdir -p $(LOCAL_MODULE_PATH)/../sys; \
			mkdir -p $(LOCAL_MODULE_PATH)/../dev; \
			mkdir -p $(LOCAL_MODULE_PATH)/../etc; \
			mkdir -p $(LOCAL_MODULE_PATH)/../usr/sbin; \
			mkdir -p $(LOCAL_MODULE_PATH)/../vendor/firmware_mnt; \
			mkdir -p $(LOCAL_MODULE_PATH)/../vendor/lib/modules; \
                        mkdir -p $(LOCAL_MODULE_PATH)/../vendor/etc; \
                        mkdir -p $(LOCAL_MODULE_PATH)/../vendor/etc/selinux; \
			ln -sf system/bin $(LOCAL_MODULE_PATH)/../bin; \
			ln -sf system/lib64 $(LOCAL_MODULE_PATH)/../lib64; \

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := early_init_eth.conf
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS = ETC
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/early_services/etc
include $(BUILD_PREBUILT)

