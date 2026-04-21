LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE                    := early_video_app
LOCAL_CPPFLAGS                  := -std=c++17
LOCAL_CPPFLAGS                  += -fexceptions
LOCAL_LDFLAGS                   := -Wl,-rpath,'/vendor_early_services/system/lib64' -Wl,--dynamic-linker,/vendor_early_services/system/bin/bootstrap/linker64
LOCAL_MODULE_PATH               := $(TARGET_VENDOR_RAMDISK_OUT)/vendor_early_services/vendor/bin
LOCAL_C_INCLUDES                += $(LOCAL_PATH)/inc
LOCAL_C_INCLUDES                += $(LOCAL_PATH)/driver/stub_driver/linux/inc/uapi

LOCAL_SHARED_LIBRARIES          := \
                                libcutils \
                                libbase \
                                libutils \
                                libion \
                                liblog \
                                libdmabufheap \
                                libdrm

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