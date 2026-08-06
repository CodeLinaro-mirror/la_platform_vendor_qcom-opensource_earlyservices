/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef __MSM_V4L2_CODEC_H__
#define __MSM_V4L2_CODEC_H__

#include <list>
#include <list>
#include <mutex>

#include "V4l2CodecCallback.h"
#include "V4l2Driver.h"
#include "VidcLog.h"

#define MAX_FRAMES 17
#define SZ_4K 0x00001000
#define SZ_2M 0x00200000
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define INVALID_VALUE -1

struct V4L2ControlInfo {
	unsigned int id = 0;
	int value = 0;
};

struct V4L2DynamicParams {
	unsigned int controlId;
	int value;
	unsigned int frameNum;
};

struct V4L2OutputFenceInfo {
public:
	V4L2OutputFenceInfo() :
		outputBufTag(INVALID_VALUE) {
	}

	int outputBufTag;
	std::list<int> fenceIds;
	std::list<int> fenceFds;
};

enum sParmType {
	SPARM_NONE           = 0,
	SPARM_FRAME_RATE     = 1,
	SPARM_OPERATING_RATE = 2,
};

void print_v4l2_buffer(const char* string, struct v4l2_buffer* b);

class V4l2Codec {
	public:
		V4l2Codec();

		virtual ~V4l2Codec() = default;
		virtual int init(unsigned int codec) = 0;
		virtual void deinit() = 0;
		virtual int configureInput() = 0;
		virtual int configureOutput() = 0;
		virtual int registerCallbacks(std::shared_ptr<V4l2CodecCb> cb) = 0;
		virtual int detectBitDepthChange() = 0;
		virtual int reconfigureOutput() = 0;
		virtual int setOperatingRate(unsigned int numer, unsigned int denom) = 0;
		virtual int setFrameRate(unsigned int numer, unsigned int denom) = 0;
		virtual int getOperatingRate() = 0;
		virtual float getFrameRate() = 0;
		virtual int drain() = 0;
		virtual int resume() = 0;
		virtual struct v4l2_format* getOutputFormat() = 0;
		virtual int getOutputImgWidth() = 0;
		virtual int getOutputImgHeight() = 0;
		virtual int getFenceFds(struct V4L2OutputFenceInfo* fenceInfo) = 0;
		virtual int getFenceFd(int fence_id) = 0;
		virtual int setDSResolution(unsigned int width, unsigned int height) = 0;

		bool isAPVSupported();
		int setStride(unsigned int stride);
		int setScanline(unsigned int scanline);
		int setColorFormat(unsigned int colorformat);
		int setWidth(unsigned int width);
		int setHeight(unsigned int height);
		int setDownscaleResolution(unsigned width, unsigned height);
		int getMinInputCount();
		int getMinOutputCount();
		int setInputCount(int count);
		int setOutputCount(int count);
		int setInputSizeOverWrite(int size);
		int setInputActualCount(int count);
		int setOutputActualCount(int count);
		int setOutputBufferRecycle(bool enable);
		int getCrop(struct v4l2_rect* crop);
		bool isInputMetadata(unsigned int controlId);
		bool isOutputMetadata(unsigned int controlId);
		int setV4l2Controls();
		int setColorSpaceInfo(enum port_type port, unsigned int colorPrimaries,
			unsigned int matrixCoeff, unsigned int transferChar, unsigned int range);
		int getColorSpaceInfo(enum port_type port, unsigned int *colorPrimaries,
			unsigned int *matrixCoeff, unsigned int *transferChar, unsigned int *range);
		int startInput();
		int startOutput();
		int stopInput();
		int stopOutput();
		int startMetaInput();
		int startMetaOutput();
		int stopMetaInput();
		int stopMetaOutput();
		int fillMetadata(std::shared_ptr<v4l2_buffer> metaBuf);
		int extractMetadata(struct v4l2_buffer* metaBuf, struct V4L2OutputFenceInfo* fenceInfo);
		int queueBuffer(std::shared_ptr<v4l2_buffer> buffer);
		int allocateMetaBuffers(enum port_type port);
		int allocateBuffers(enum port_type port);
		int allocateBuffersSingleFd(enum port_type port);
		void freeMetaBuffers(enum port_type port);
		void freeBuffers(enum port_type port);
		void freeBuffersSingleFd(enum port_type port);
		int ionAlloc(int size);
		void ionFree(int fd);
		int setControl(unsigned int ctrlId, int value);
		int queryControl(struct v4l2_queryctrl *queryctrl);
		int queryMenu(struct v4l2_querymenu *querymenu);
		void setEarlyNotifyIntrptCount(uint32_t count);
		std::shared_ptr<v4l2_buffer> allocateBuffer(int index, enum port_type port, int bufSize);
		std::shared_ptr<v4l2_buffer> allocateMetaBuffer(int index, enum port_type port, int bufSize);
		int queueBuffer(std::shared_ptr<v4l2_buffer> buffer, unsigned int currentFrameNum);
		int queueBufferRequest(std::shared_ptr<v4l2_buffer> buffer, unsigned int currentFrameNum);

		int setCvpMetaFile(std::string cvpFile);
		int setProfile(int profile);
		/* Get member params */
		int getInputSize();
		int getMetaInputSize();
		int getOutputSize();
		int getMetaOutputSize();
		int getOutputMinCount();
		int getOutputAllocCount();
		int setMultiplier(const unsigned int& cnt);
		unsigned int getMultiplier();
		void setCompleteFrame(bool fullFrame);
		bool isCompleteFrame();
		void setFrameCount(int frameCount);
		bool isInputMetadataEnabled();
		bool isOutputMetadataEnabled();
		bool isInputRequestEnabled();
		bool isOutBufFenceEnabled();

		bool mOutputStreamonDone;
		bool mFirstReconfigReceived;
		std::mutex mInputBufLock;
		std::mutex mOutputBufLock;
		std::list<std::shared_ptr<V4L2ControlInfo>> mControls;
		std::list<std::shared_ptr<V4L2ControlInfo>> mMetaControls;
		std::list<std::shared_ptr<v4l2_buffer>> mInputBufs;
		std::list<std::shared_ptr<v4l2_buffer>> mOutputBufs;
		std::list<std::shared_ptr<v4l2_buffer>> mMetaInputBufs;
		std::list<std::shared_ptr<v4l2_buffer>> mMetaOutputBufs;
		std::list<std::shared_ptr<v4l2_buffer>> mPendingOutputBufs;
		std::list<std::shared_ptr<v4l2_buffer>> mPendingInputBufs;
		std::list<std::shared_ptr<v4l2_buffer>> mPendingMetaOutputBufs;
		std::list<std::shared_ptr<v4l2_buffer>> mPendingMetaInputBufs;
		std::list<std::shared_ptr<V4L2DynamicParams>> mDynamicParams;
		std::list<std::shared_ptr<struct V4L2OutputFenceInfo>> mOutBufFenceList;
		std::shared_ptr<V4l2Driver> mV4l2Driver;

	protected:
		friend class V4l2Callback;
		struct v4l2_format mOutputFormat;
		std::shared_ptr<V4l2CodecCb> mCb;
		unsigned int mCodec = 0;
		unsigned int mDomain = 0;
		int mSecure = 0;
		int mThumbnail = 0;
		int mMinInputCount = 0;
		int mActualInputCount = 0;
		int mMinOutputCount = 0;
		int mActualOutputCount = 0;
		bool mOutputBufferRecycle = true;
		int mInputSize = 0;
		int mInputSizeOverWrite = 0;
		int mMetaInputSize = 0;
		int mOutputSize = 0;
		int mMetaOutputSize = 0;
		int mLowLatencyMode = 0;
		int mProfile = 0;
		int mLevel = 0;
		int mTier = 0;
		int mCropLeft = 0;
		int mCropTop = 0;
		int mCropWidth = 0;
		int mCropHeight = 0;
		unsigned int mMultiplier = 1;
		int mWidth = 0;
		int mHeight = 0;
		int mOutputImgWidth = 0;
		int mOutputImgHeight = 0;
		int mStride = 0;
		int mScanline = 0;
		int mDownscaleWidth = 0;
		int mDownscaleHeight = 0;
		bool mDownscaleEnable = false;
		float mFrameRate = 0;
		int mBitrate = 0;
		int mBitrateMode = 0;
		unsigned int mColorFormat = 0;
		unsigned int mInputColorPrimaries = V4L2_COLORSPACE_DEFAULT;
		unsigned int mInputMatrixCoeff = V4L2_YCBCR_ENC_DEFAULT;
		unsigned int mInputTransferChar = V4L2_XFER_FUNC_DEFAULT;
		unsigned int mInputVideoRange = V4L2_QUANTIZATION_DEFAULT;
		unsigned int mOutputColorPrimaries = V4L2_COLORSPACE_DEFAULT;
		unsigned int mOutputMatrixCoeff = V4L2_YCBCR_ENC_DEFAULT;
		unsigned int mOutputTransferChar = V4L2_XFER_FUNC_DEFAULT;
		unsigned int mOutputVideoRange = V4L2_QUANTIZATION_DEFAULT;
		int mIonFd = -1;
		bool mIsCompleteFrame = true;
		int mFrameCount = -1;
		bool mFgPresent = false;

		FILE *mCvpMetaFile = nullptr;
		// Array of MISR data for the bitstream 'TSKIP_A_MS_3.bit'
		// format: DPB Luma Misr[i], DPB Chroma Misr[i + 1] for pipes 0-3
		// no of pipes = 4, frames = 0-16
		int mMisrFrame = 0;
		static constexpr const uint8_t kNumPipes = 4;
		static constexpr const uint32_t kMisrArr[MAX_FRAMES][8] = {
			{0x6f8ed678, 0xd26f93d7, 0x4c2a7416, 0x00d98a8f, 0x81682519, 0xa31f29c7, 0xfce938ca, 0x9c75de11},
			{0x27bd496f, 0xb5f33729, 0x63b57988, 0x6131f6d7, 0x8151c1a6, 0x973f322c, 0x5d1e4c3b, 0x8d550abf},
			{0x3433ec98, 0xacf0766b, 0x4f4778bd, 0x13520624, 0x2f9e5721, 0x049b337f, 0x766b7909, 0xc70e5999},
			{0xb5a10e2b, 0x4a1d83f5, 0x265db2a9, 0x4e5d2ac1, 0x35d0cf42, 0xae249380, 0x7b16d6dd, 0x8d550abf},
			{0x889b267c, 0xb6655c5a, 0xb4a3011a, 0x39ae4ef8, 0x92ac901d, 0xd90f3fbf, 0x4a79f6c7, 0x445dbce5},
			{0x425373e1, 0x627ca149, 0x5801429e, 0x2fb98df8, 0xc128b593, 0x2aff56f1, 0x3d844358, 0x7c18aa36},
			{0xdbf83247, 0x15ef4d8d, 0x54ee3714, 0x54cea648, 0x3f04823c, 0x098e4b31, 0x98dface9, 0x5ea9120a},
			{0x3009f917, 0xe4647c86, 0x473f15d2, 0x46a268d5, 0xb7f6e803, 0x3a0b7d92, 0x3be855eb, 0x15084a22},
			{0xb8796e0c, 0x5b35f1f0, 0x408459e9, 0xaffbfcd1, 0x14a917a2, 0x167f4caa, 0x3e44b839, 0x92058609},
			{0x3d61cf6c, 0x7b9f15b3, 0x4ce9f281, 0x8420effe, 0x3110c9a4, 0xa4e2a2b6, 0xf24548a8, 0x799d1f8e},
			{0x5374c297, 0x47edefe3, 0x1b7f4a73, 0x4b274920, 0x7daa377f, 0x15d28c1f, 0x80496bba, 0xc444ef89},
			{0xdcf9b84e, 0xee196031, 0xba5d227d, 0xe6559821, 0x889111a3, 0x941bf5b5, 0xaa9ecc98, 0xe9f829c3},
			{0x71731e9d, 0x89fd87fc, 0xc03ae7e5, 0xb093cf88, 0x854521cc, 0xe45d44cf, 0xcef45a11, 0xaf95e7f7},
			{0x29038183, 0x94ad3237, 0x304da88d, 0xadfff10d, 0xbf43ed5d, 0xa3a13304, 0xe46611cb, 0x6377f196},
			{0x4979c52d, 0x608042ec, 0x97f32aa9, 0xeee268f6, 0xdfb5226b, 0xb889f22f, 0x7d87cb85, 0xe1633ab8},
			{0x468b49a5, 0x63d154db, 0x3a85c584, 0xa2ea4ae1, 0xa97cc924, 0xcc6ae551, 0x1c85cd64, 0x17fce489},
			{0xe00ff60b, 0x63d154db, 0x62fa7008, 0xa2ea4ae1, 0xedcdce61, 0x83e0cda7, 0x80a0f3f6, 0x043d70e6},
		};
};

#endif
