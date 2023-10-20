LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES:= early_init.cpp \
                  util.cpp \
                  log.cpp

ifneq ($(filter sdmshrike msmnile,$(TARGET_BOARD_PLATFORM)),)
	LOCAL_CFLAGS := -DPLATFORM_MSMNILE
endif
ifneq ($(filter $(MSMSTEPPE),$(TARGET_BOARD_PLATFORM)),)
	LOCAL_CFLAGS := -DPLATFORM_MSMSTEPPE
endif

ifneq (,$(filter U 14 UpsideDownCake, $(PLATFORM_VERSION)))
LOCAL_CFLAGS += -D__ANDROID_U__
endif

LOCAL_MODULE := early_services_init
#LOCAL_STATIC_LIBRARIES := libc++_static
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_PATH := $(TARGET_RAMDISK_OUT)/vendor_early_services/bin
LOCAL_POST_INSTALL_CMD := $(hide) mkdir -p $(TARGET_ROOT_OUT)/vendor_early_services; \
                                  mkdir -p $(TARGET_ROOT_OUT)/vendor_early_services/dev; \
                                  mkdir -p $(TARGET_RAMDISK_OUT)/vendor_early_services;

LOCAL_STATIC_LIBRARIES := libc++_static \
     libbase \
     libcrypto_utils \
     libcutils\
     liblog \
     libseccomp_policy \
     libselinux \
     libfs_mgr \
     libmodprobe \

LOCAL_CPPFLAGS := -std=c++17
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
#To be removed later
LOCAL_MODULE_TAGS := optional
LOCAL_LDFLAGS := -Wl,-rpath,'/vendor_early_services/system/lib64' -Wl,--dynamic-linker,/vendor_early_services/system/bin/bootstrap/linker64
LOCAL_MODULE := init_early_gpio_test
LOCAL_SRC_FILES := test_gpio.c
LOCAL_MODULE_CLASS = ETC
LOCAL_MODULE_PATH := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/system/bin
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
#To be removed later
LOCAL_MODULE_TAGS := optional
LOCAL_LDFLAGS := -Wl,-rpath,'/vendor_early_services/system/lib64' -Wl,--dynamic-linker,/vendor_early_services/system/bin/bootstrap/linker64
LOCAL_MODULE := init_early_spi_test
LOCAL_SRC_FILES := test_spi.c
LOCAL_MODULE_CLASS = ETC
LOCAL_MODULE_PATH := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/system/bin
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

# static linking start
#LOCAL_MODULE_TAGS := optional
#LOCAL_SRC_FILES := test.c
#LOCAL_MODULE := init_early_test
#LOCAL_STATIC_LIBRARIES := libc++_static
#LOCAL_FORCE_STATIC_EXECUTABLE := true
#LOCAL_MODULE_PATH := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/system/bin
#LOCAL_CPPFLAGS := -std=c++17
# static linking end

# dynamic linking start

#To be removed later
LOCAL_MODULE_TAGS := optional
LOCAL_LDFLAGS := -Wl,-rpath,'/vendor_early_services/system/lib64' -Wl,--dynamic-linker,/vendor_early_services/system/bin/bootstrap/linker64
LOCAL_MODULE := init_early_test
LOCAL_SRC_FILES := test.c
LOCAL_HEADER_LIBRARIES += libcutils_headers
LOCAL_SHARED_LIBRARIES:=  libselinux
LOCAL_MODULE_PATH := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/bin
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := early_init.conf
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS = ETC
LOCAL_MODULE_PATH := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/etc
#Prepare Early Rootfs structure
LOCAL_POST_INSTALL_CMD := $(hide) mkdir -p $(LOCAL_MODULE_PATH)/../sbin; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../system/bin; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../system/lib64; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../system/etc; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../vendor/lib64; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../system/etc/selinux; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../run/early; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../proc; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../sys; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../dev; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../etc; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../usr/sbin; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../vendor/firmware_mnt; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../vendor/etc; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../vendor/etc/selinux; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../vendor/lib; \
                                  mkdir -p $(LOCAL_MODULE_PATH)/../vendor/lib/modules; \
                                  mkdir -p $(TARGET_VENDOR_RAMDISK_OUT)/aes_tmpfs; \
                                  ln -sf system/bin $(LOCAL_MODULE_PATH)/../bin; \
                                  ln -sf system/lib64 $(LOCAL_MODULE_PATH)/../lib64; \

include $(BUILD_PREBUILT)


include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := early_init_eth.conf
LOCAL_SRC_FILES := $(LOCAL_MODULE)
LOCAL_MODULE_CLASS = ETC
LOCAL_MODULE_PATH := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/etc
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_PATH := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/vendor/bin
ifneq ($(filter sdmshrike msmnile,$(TARGET_BOARD_PLATFORM)),)
	LOCAL_CFLAGS := -DPLATFORM_MSMNILE
endif
ifneq ($(filter $(MSMSTEPPE),$(TARGET_BOARD_PLATFORM)),)
	LOCAL_CFLAGS := -DPLATFORM_MSMSTEPPE
endif
LOCAL_LDFLAGS := -Wl,-rpath,'/vendor_early_services/system/lib64' -Wl,--dynamic-linker,/vendor_early_services/system/bin/bootstrap/linker64
LOCAL_C_INCLUDES:= hardware/libhardware/include \
                    system/media/audio/include \
                    external/tinycompress/include \
                    $(call include-path-for, audio-route) \
                    system/media/audio_utils/include \

LOCAL_HEADER_LIBRARIES += libhardware_headers
LOCAL_HEADER_LIBRARIES += libcutils_headers
LOCAL_SRC_FILES:= early_chime_app.c \
		  early_audiod.c
LOCAL_MODULE := early_chime
LOCAL_SHARED_LIBRARIES:=  libtinyalsa
include $(BUILD_EXECUTABLE)
