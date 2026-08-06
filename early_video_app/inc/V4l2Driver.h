/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#pragma once

#ifndef __MSM_V4L2_DRIVER_H__
#define __MSM_V4L2_DRIVER_H__

#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>

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

#include "../driver/stub_driver/linux/inc/uapi/vidc/media/v4l2_vidc_extensions.h"


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

enum v4l2_mpeg_vidc_metadata_bits {
	V4L2_MPEG_VIDC_META_DISABLE          = 0x0,
	V4L2_MPEG_VIDC_META_ENABLE           = 0x1,
	V4L2_MPEG_VIDC_META_TX_INPUT         = 0x2,
	V4L2_MPEG_VIDC_META_TX_OUTPUT        = 0x4,
	V4L2_MPEG_VIDC_META_RX_INPUT         = 0x8,
	V4L2_MPEG_VIDC_META_RX_OUTPUT        = 0x10,
	V4L2_MPEG_VIDC_META_DYN_ENABLE       = 0x20,
	V4L2_MPEG_VIDC_META_MAX              = 0x40,
};

enum v4l2_mpeg_vidc_metapayload_header_flags {
	METADATA_FLAGS_NONE             = 0,
	METADATA_FLAGS_TOP_FIELD        = (1 << 0),
	METADATA_FLAGS_BOTTOM_FIELD     = (1 << 1),
	METADATA_FLAGS_BITSTREAM        = (1 << 2),
	METADATA_FLAGS_RAW              = (1 << 3),
};

enum saliency_roi_info {
	METADATA_SALIENCY_NONE,
	METADATA_SALIENCY_TYPE0,
};

struct msm_vidc_metabuf_header {
	__u32 count;
	__u32 size;
	__u32 version;
	__u32 reserved[5];
};

struct msm_vidc_metapayload_header {
	__u32 type;
	__u32 size;
	__u32 version;
	__u32 offset;
	__u32 flags;
	__u32 reserved[3];
};

enum v4l2_mpeg_vidc_metadata {
	METADATA_BITSTREAM_RESOLUTION         = 0x03000103,
	METADATA_CROP_OFFSETS                 = 0x03000105,
	METADATA_LTR_MARK_USE_DETAILS         = 0x03000137,
	METADATA_SEQ_HEADER_NAL               = 0x0300014a,
	METADATA_DPB_LUMA_CHROMA_MISR         = 0x03000153,
	METADATA_OPB_LUMA_CHROMA_MISR         = 0x03000154,
	METADATA_INTERLACE_INFO               = 0x03000156,
	METADATA_TIMESTAMP                    = 0x0300015c,
	METADATA_CONCEALED_MB_COUNT           = 0x0300015f,
	METADATA_HISTOGRAM_INFO               = 0x03000161,
	METADATA_PICTURE_TYPE                 = 0x03000162,
	METADATA_SEI_MASTERING_DISPLAY_COLOUR = 0x03000163,
	METADATA_SEI_CONTENT_LIGHT_LEVEL      = 0x03000164,
	METADATA_SEI_HDR10PLUS_USERDATA       = 0x03000165,
	METADATA_EVA_STAT_INFO                = 0x03000167,
	METADATA_BUFFER_TAG                   = 0x0300016b,
	METADATA_SUBFRAME_OUTPUT              = 0x0300016d,
	METADATA_ENC_QP_METADATA              = 0x0300016e,
	METADATA_DEC_QP_METADATA              = 0x0300016f,
	METADATA_ROI_INFO                     = 0x03000173,
	METADATA_DPB_TAG_LIST                 = 0x03000179,
	METADATA_MAX_NUM_REORDER_FRAMES       = 0x03000127,
	METADATA_ROI_AS_SALIENCY_INFO         = 0x0300018A,
	METADATA_FENCE_OUTPUT                 = 0x0300018B,
	METADATA_TRANSCODING_STAT_INFO        = 0x03000191,
	METADATA_DOLBY_RPU                    = 0x03000192,
	METADATA_MULTI_VIEW                   = 0x030001A5,
};

enum meta_interlace_info {
	META_INTERLACE_INFO_NONE                            = 0x00000000,
	META_INTERLACE_FRAME_PROGRESSIVE                    = 0x00000001,
	META_INTERLACE_FRAME_MBAFF                          = 0x00000002,
	META_INTERLACE_FRAME_INTERLEAVE_TOPFIELD_FIRST      = 0x00000004,
	META_INTERLACE_FRAME_INTERLEAVE_BOTTOMFIELD_FIRST   = 0x00000008,
	META_INTERLACE_FRAME_INTERLACE_TOPFIELD_FIRST       = 0x00000010,
	META_INTERLACE_FRAME_INTERLACE_BOTTOMFIELD_FIRST    = 0x00000020,
};

/*
 * enum meta_picture_type - specifies input picture type
 * @META_PICTURE_TYPE_NEW: start of new frame or first slice in a frame
 */
enum meta_picture_type {
	META_PICTURE_TYPE_IDR                            = 0x00000001,
	META_PICTURE_TYPE_P                              = 0x00000002,
	META_PICTURE_TYPE_B                              = 0x00000004,
	META_PICTURE_TYPE_I                              = 0x00000008,
	META_PICTURE_TYPE_CRA                            = 0x00000010,
	META_PICTURE_TYPE_BLA                            = 0x00000020,
	META_PICTURE_TYPE_NOSHOW                         = 0x00000040,
	META_PICTURE_TYPE_NEW                            = 0x00000080,
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
		void onOutputPollEvent(v4l2_event& event);
		int outputThreadLoop();
		int createPollOutputThread();
		int stopPollOutputThread();
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
		std::shared_ptr<std::thread> mPollOutputThread;
        std::vector<v4l2_event> mPollOutputEventVector;
		std::mutex mOutputEventVectorMutex;
		std::condition_variable mOutputEventVectorSignal;
		std::atomic<bool> mThreadRunning{false};
		std::atomic<bool> mPollThreadExit{false};
		std::atomic<bool> mOutputThreadRunning{false};
		std::atomic<bool> mPollOutputThreadExit{false};
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
