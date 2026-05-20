LOCAL_PATH                      := $(call my-dir)
include $(CLEAR_VARS)
ifneq ($(filter volcano, $(TARGET_BOARD_PLATFORM)),)
# Disable this modules temp to volcano BU
        LOCAL_CFLAGS := -DPLATFORM_VOLCANO
else
LOCAL_MODULE                    := early_video_app
LOCAL_CPPFLAGS                  := -std=c++17
LOCAL_CPPFLAGS                  += -fexceptions
LOCAL_LDFLAGS                   := -Wl,-rpath,'/vendor_early_services/system/lib64' -Wl,--dynamic-linker,/vendor_early_services/system/bin/bootstrap/linker64
LOCAL_MODULE_PATH               := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/bin
LOCAL_C_INCLUDES                += $(LOCAL_PATH)/inc

LOCAL_SHARED_LIBRARIES          := \
                                libcutils \
                                libbase \
                                libutils \
                                libion \
                                liblog \
                                libdmabufheap \
                                libdrm

LOCAL_HEADER_LIBRARIES          += qti_display_kernel_headers

LOCAL_SRC_FILES                 := \
                                main.cpp \
                                GTDecoder.cpp \
                                src/V4l2Callback.cpp \
                                src/V4l2Codec.cpp \
                                src/V4l2Driver.cpp \
                                src/V4l2Decoder.cpp \
                                src/GTDecoderIOAdapter.cpp \
                                src/EventHandler.cpp \
                                src/GTCodec.cpp \
                                src/GTDecoderCallback.cpp \
                                src/VidcLog.cpp \
                                src/Utils.cpp \
                                render/DisplayAdaptor.cpp
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE                    := early_video_res_primary
LOCAL_SRC_FILES                 := resource/primary.h264
LOCAL_MODULE_CLASS              := ETC
LOCAL_MODULE_PATH               := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/vendor/data/early_video
LOCAL_INSTALLED_MODULE_STEM     := primary.h264
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE                    := early_video_res_primary_config
LOCAL_SRC_FILES                 := resource/primary.config
LOCAL_MODULE_CLASS              := ETC
LOCAL_MODULE_PATH               := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/vendor/data/early_video
LOCAL_INSTALLED_MODULE_STEM     := primary.config
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE                    := early_video_res_secondary
LOCAL_SRC_FILES                 := resource/secondary.h264
LOCAL_MODULE_CLASS              := ETC
LOCAL_MODULE_PATH               := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/vendor/data/early_video
LOCAL_INSTALLED_MODULE_STEM     := secondary.h264
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE                    := early_video_res_secondary_config
LOCAL_SRC_FILES                 := resource/secondary.config
LOCAL_MODULE_CLASS              := ETC
LOCAL_MODULE_PATH               := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/vendor/data/early_video
LOCAL_INSTALLED_MODULE_STEM     := secondary.config
include $(BUILD_PREBUILT)

endif