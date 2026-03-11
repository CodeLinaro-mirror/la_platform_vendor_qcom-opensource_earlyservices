/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#pragma once

#ifndef __MSM_V4L2_DRIVER_H__
#define __MSM_V4L2_DRIVER_H__

#include <thread>

#ifdef ANDROID
#include <linux/msm_ion.h>
#include <linux/ion.h>
#include <ion/ion.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#endif
#include <sys/mman.h>
#include <linux/videodev2.h>

#include "vidc/media/v4l2_vidc_extensions.h"


/* start of vidc specific colorspace definitions */
/*
 * V4L2_COLORSPACE_VIDC_START, V4L2_XFER_FUNC_VIDC_START
 * and V4L2_YCBCR_VIDC_START are introduced because
 * V4L2_COLORSPACE_LAST, V4L2_XFER_FUNC_LAST, and
 * V4L2_YCBCR_ENC_LAST respectively are not accessible
 * in userspace. These values are needed in userspace
 * to check if the colorspace info is private.
 */
#define V4L2_COLORSPACE_VIDC_START           100
#define V4L2_COLORSPACE_VIDC_GENERIC_FILM    101
#define V4L2_COLORSPACE_VIDC_EG431           102
#define V4L2_COLORSPACE_VIDC_EBU_TECH        103

#define V4L2_XFER_FUNC_VIDC_START            200
#define V4L2_XFER_FUNC_VIDC_BT470_SYSTEM_M   201
#define V4L2_XFER_FUNC_VIDC_BT470_SYSTEM_BG  202
#define V4L2_XFER_FUNC_VIDC_BT601_525_OR_625 203
#define V4L2_XFER_FUNC_VIDC_LINEAR           204
#define V4L2_XFER_FUNC_VIDC_XVYCC            205
#define V4L2_XFER_FUNC_VIDC_BT1361           206
#define V4L2_XFER_FUNC_VIDC_BT2020           207
#define V4L2_XFER_FUNC_VIDC_ST428            208
#define V4L2_XFER_FUNC_VIDC_HLG              209

/* should be 255 or below due to u8 limitation */
#define V4L2_YCBCR_VIDC_START                240
#define V4L2_YCBCR_VIDC_SRGB_OR_SMPTE_ST428  241
#define V4L2_YCBCR_VIDC_FCC47_73_682         242
/* end of vidc specific colorspace definitions */

enum codec_type {
    V4L2_CODEC_TYPE_DECODER = 1,
    V4L2_CODEC_TYPE_ENCODER,
};

/* Encoder Slice Delivery Mode
 * set format has a dependency on this control
 * and gets invoked when this control is updated.
 */
#define V4L2_CID_MPEG_VIDC_HEVC_ENCODE_DELIVERY_MODE                          \
    (V4L2_CID_MPEG_VIDC_BASE + 0x3C)

#define V4L2_CID_MPEG_VIDC_H264_ENCODE_DELIVERY_MODE                          \
    (V4L2_CID_MPEG_VIDC_BASE + 0x3D)

#define V4L2_CID_MPEG_VIDC_CRITICAL_PRIORITY                                 \
    (V4L2_CID_MPEG_VIDC_BASE + 0x3E)
#define V4L2_CID_MPEG_VIDC_RESERVE_DURATION                                  \
    (V4L2_CID_MPEG_VIDC_BASE + 0x3F)

#define INPUT_MPLANE V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
#define OUTPUT_MPLANE V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
#define INPUT_META_PLANE V4L2_BUF_TYPE_META_OUTPUT
#define OUTPUT_META_PLANE V4L2_BUF_TYPE_META_CAPTURE

enum port_type {
    INPUT_PORT = 0,
    OUTPUT_PORT,
    INPUT_META_PORT,
    OUTPUT_META_PORT,
    MAX_PORT,
};

class InputTag {
    public:
        static constexpr uint32_t INVALID = 0;
        static inline constexpr uint32_t fromId(uint64_t id) {
            return id >= (UINT32_MAX - kOffset) ? INVALID : id + kOffset;
        }
        static inline constexpr uint64_t toId(uint32_t tag) {
            return tag == 0 ? UINT32_MAX : tag - kOffset;
        }
    private:
        static constexpr uint32_t kOffset = 1;
};

class OutputTag {
    public:
        static constexpr uint32_t INVALID = 0;
        static inline constexpr uint32_t fromId(uint64_t id) {
            return id >= (UINT32_MAX - kOffset) ? INVALID : id + kOffset;
        }
        static inline constexpr uint64_t toId(uint32_t tag) {
            return tag == 0 ? UINT32_MAX : tag - kOffset;
        }
    private:
        static constexpr uint32_t kOffset = 50;
};

class V4l2DriverCb {
    public:
        virtual ~V4l2DriverCb() = default;
        virtual int onV4l2BufferDone(struct v4l2_buffer* buffer) = 0;
        virtual void onV4l2EventDone(struct v4l2_event* event) = 0;
        virtual int onV4l2Error(int error) = 0;
};

class V4l2Driver {
    public:
        bool mError = false;
        int Open(unsigned int type);
        int openMediaDevice(unsigned int type);
        void Close();
        void closeMediaDevice();
        int queryControl(struct v4l2_queryctrl* ctrl);
        int queryMenu(struct v4l2_querymenu* ctrl);
        int queryCapabilities(struct v4l2_capability *caps);
        int enumFormat(struct v4l2_fmtdesc* fmtdesc);
        int enumFramesize(struct v4l2_frmsizeenum* frmsize);
        int enumFrameInterval(struct v4l2_frmivalenum *fival);
        int subscribeEvent(struct v4l2_event_subscription event);
        int unsubscribeEvent(struct v4l2_event_subscription event);
        int allocateRequestFd(int port);
        int closeRequestFd(int port);
        int streamOn(int port);
        int streamOff(int port);
        int getFormat(struct v4l2_format* fmt);
        int setFormat(struct v4l2_format* fmt);
        int setParm(struct v4l2_streamparm* sparm);
        int getParm(struct v4l2_streamparm* sparm);
        int setControl(struct v4l2_control* ctrl);
        int getControl(struct v4l2_control* ctrl);
        int setExtControls(struct v4l2_ext_controls* ctrls);
        int getSelection(struct v4l2_selection* fmt);
        int setSelection(struct v4l2_selection* fmt);
        int reqBufs(struct v4l2_requestbuffers* reqBufs);
        int queueBuf(v4l2_buffer* buf);
        int deQueueBuf(struct v4l2_buffer* buffer);
        void dequeueInputMetaBuffersEarly();
        int queueBufferRequest(struct v4l2_buffer *b, struct v4l2_ext_controls *controls);
        int decCommand(struct v4l2_decoder_cmd* cmd);
        int encCommand(struct v4l2_encoder_cmd* cmd);
        int subscribeEvent(unsigned int event_type);
        int unsubscribeEvent(unsigned int event_type);
        int registerCallbacks(std::shared_ptr<V4l2DriverCb> cb);
        int threadLoop();
        int createPollThread();
        int stopPollThread();
        int ionAlloc(int size);
        void ionFree(int fd);
        void setEarlyNotifyIntrptCount(uint32_t count);
        uint32_t getEarlyNotifyIntrptCount();
        void enableInputRequest(bool enable);
        bool isInputRequestEnabled();
        void enableInputMetaPort(bool enable);
        bool isInputMetaPortEnabled();
        void enableOutputMetaPort(bool enable);
        bool isOutputMetaPortEnabled();
        void enableInputMetadata(bool enable);
        bool isInputMetadataEnabled();
        void enableOutputMetadata(bool enable);
        bool isOutputMetadataEnabled();
        void enableOutBufFence(bool enable);
        bool isOutBufFenceEnabled();
        
    private:
        int mFd = -1;
        int mIonFd = -1;
        int mMediaFd = -1;
        int mRequestFd[VIDEO_MAX_FRAME] = {-1};
        std::shared_ptr<std::thread> mPollThread;
        bool mThreadRunning = false;
        bool mPollThreadExit = false;
        std::shared_ptr<V4l2DriverCb> mCb;
        bool mInputRequestEnabled = false;
        bool mInputMetaPortEnabled = false;
        bool mOutputMetaPortEnabled = false;
        bool mInputMetadataEnabled = false;
        bool mOutputMetadataEnabled = false;
        bool mOutBufFenceEnabled = false;
        uint32_t mEarlyNotifyIntrptCount = 0;
        std::mutex mRequestLock;
};

void print_v4l2_buffer(const char* string, struct v4l2_buffer* b);

#endif
