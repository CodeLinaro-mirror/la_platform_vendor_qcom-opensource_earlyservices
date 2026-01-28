/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifdef _LINUX_VENV_
#include <sys/mman.h>
#endif

#include "../inc/V4l2Codec.h"
#include "../inc/VidcLog.h"

using namespace early_video_app;

#define MAX_META_DELIVERY_PAYLOADS	6
#define CVP_METADATA_SIZE  		1080
#define MAX_EXT_CONTROLS 32
#define SALIENCY_METADATA_SIZE 64

V4l2Codec::V4l2Codec() {
	VIDC_HIGH("V4l2Codec, constructor\n");
	mV4l2Driver = std::make_shared<V4l2Driver>();
}

int V4l2Codec::setCvpMetaFile(std::string cvpFile) {
	mCvpMetaFile = fopen(cvpFile.c_str(), "r+b");
	VIDC_MED("V4l2Codec::setCvpMetaFile, CVP Meta input file name:%s\n",cvpFile.c_str());
	if (!mCvpMetaFile) {
		VIDC_ERR("V4l2Codec::setCvpMetaFile, CVPMeta file open failed\n");
		return -1;
	}
	return 0;
}

/* Get member params */
int V4l2Codec::getInputSize() {
	return mInputSize;
}

int V4l2Codec::getMetaInputSize() {
	return mMetaInputSize;
}

int V4l2Codec::getOutputSize() {
	return mOutputSize;
}

int V4l2Codec::getMetaOutputSize() {
	return mMetaOutputSize;
}

int V4l2Codec::getOutputMinCount() {
	return mMinOutputCount;
}

int V4l2Codec::getOutputAllocCount() {
	return mOutputBufferRecycle ? mMinOutputCount :
		mActualOutputCount;
}

int V4l2Codec::setMultiplier(const unsigned int& cnt) {
	mMultiplier = cnt;
	return 0;
}

unsigned int V4l2Codec::getMultiplier() {
	return mMultiplier;
}

void V4l2Codec::setCompleteFrame(bool fullFrame) {
	mIsCompleteFrame = fullFrame;
}

bool V4l2Codec::isCompleteFrame() {
	return mIsCompleteFrame;
}

void V4l2Codec::setFrameCount(int frameCount) {
	mFrameCount = frameCount;
}

bool V4l2Codec::isInputMetadataEnabled() {
	return mV4l2Driver->isInputMetadataEnabled();
}

bool V4l2Codec::isOutputMetadataEnabled() {
	return mV4l2Driver->isOutputMetadataEnabled();
}

bool V4l2Codec::isInputRequestEnabled() {
	return mV4l2Driver->isInputRequestEnabled();
}

bool V4l2Codec::isOutBufFenceEnabled() {
	return mV4l2Driver->isOutBufFenceEnabled();
}

int V4l2Codec::setStride(unsigned int stride) {
	VIDC_MED("V4l2Codec::setStride, client stride %d\n", stride);
	mStride = stride;
	return 0;
}

int V4l2Codec::setScanline(unsigned int scanline) {
	VIDC_MED("V4l2Codec::setScanline, client scanline %d\n", scanline);
	mScanline = scanline;
	return 0;
}

int V4l2Codec::setColorFormat(unsigned int colorformat) {
	VIDC_MED("V4l2Codec::setColorFormat, client color format %#x\n", colorformat);
	mColorFormat = colorformat;
	return 0;
}

int V4l2Codec::setWidth(unsigned int width) {
	VIDC_MED("V4l2Codec::setWidth, client width %#x\n", width);
	mWidth = width;
	return 0;
}

int V4l2Codec::setHeight(unsigned int height) {
	VIDC_MED("V4l2Codec::setHeight, client height %#x\n", height);
	mHeight = height;
	return 0;
}

int V4l2Codec::setDownscaleResolution(unsigned int width, unsigned int height) {
	VIDC_MED("V4l2Codec::setDownscaleResolution, client downscale width %#x height %#x\n", width, height);
	mDownscaleWidth = width;
	mDownscaleHeight = height;
	mDownscaleEnable = true;
	return 0;
}

int V4l2Codec::getMinInputCount() {
	return mMinInputCount;
}

int V4l2Codec::getMinOutputCount() {
	return mMinOutputCount;
}

int V4l2Codec::setInputCount(int count) {
	VIDC_MED("V4l2Codec::setInputCount, client input count %d\n", count);
	mActualInputCount = count;

	return 0;
}

int V4l2Codec::setOutputCount(int count) {
	VIDC_MED("V4l2Codec::setOutputCount, client output count %d\n", count);
	mActualOutputCount = count;

	return 0;
}

int V4l2Codec::setInputSizeOverWrite(int size) {
	VIDC_MED("V4l2Codec::setInputSizeOverWrite, Client Input Size OverWrite %d\n", size);
	mInputSizeOverWrite = size;

	return 0;
}

int V4l2Codec::setInputActualCount(int count) {
	VIDC_MED("V4l2Codec::setInputActualCount, Client set input actual count %d\n", count);
	mActualInputCount = count;

	return 0;
}

int V4l2Codec::setOutputActualCount(int count) {
	VIDC_MED("V4l2Codec::setOutputActualCount, Client set output actual count %d\n", count);
	mActualOutputCount = count;

	return 0;
}

int V4l2Codec::setOutputBufferRecycle(bool enable) {
	VIDC_MED("V4l2Codec::setOutputBufferRecycle, Output buffer reuse %d\n", enable);
	mOutputBufferRecycle = enable;

	return 0;
}

int V4l2Codec::setColorSpaceInfo(enum port_type port, unsigned int colorPrimaries,
	unsigned int matrixCoeff, unsigned int transferChar, unsigned int range) {
	if (port == INPUT_PORT) {
		mInputColorPrimaries = colorPrimaries;
		mInputMatrixCoeff = matrixCoeff;
		mInputTransferChar = transferChar;
		mInputVideoRange = range;
	}
	else if (port == OUTPUT_PORT) {
		mOutputColorPrimaries = colorPrimaries;
		mOutputMatrixCoeff = matrixCoeff;
		mOutputTransferChar = transferChar;
		mOutputVideoRange = range;
	}
	else {
		VIDC_ERR("V4l2Codec::setColorSpaceInfo, invalid port %d\n", port);
		return -EINVAL;
	}
	return 0;
}

int V4l2Codec::getColorSpaceInfo(enum port_type port, unsigned int *colorPrimaries,
	unsigned int *matrixCoeff, unsigned int *transferChar, unsigned int *range) {
	if (port != INPUT_PORT && port != OUTPUT_PORT) {
		VIDC_ERR("V4l2Codec::getColorSpaceInfo, invalid port %d\n", port);
		return -EINVAL;
	}

	if (port == INPUT_PORT) {
		*colorPrimaries = mInputColorPrimaries;
		*matrixCoeff = mInputMatrixCoeff;
		*transferChar = mInputTransferChar;
		*range = mInputVideoRange;
	}
	else if (port == OUTPUT_PORT) {
		*colorPrimaries = mOutputColorPrimaries;
		*matrixCoeff = mOutputMatrixCoeff;
		*transferChar = mOutputTransferChar;
		*range = mOutputVideoRange;
	}

	return 0;
}

int V4l2Codec::setProfile(int profile) {
	VIDC_MED("V4l2Codec::setProfile, setting profile %d\n", profile);
	mProfile = profile;
	return 0;
}

int V4l2Codec::startInput() {
	if (mV4l2Driver->isInputRequestEnabled())
		if (mV4l2Driver->allocateRequestFd(INPUT_MPLANE))
			return -EINVAL;

	if (mV4l2Driver->streamOn(INPUT_MPLANE))
		return -EINVAL;
	return 0;
}

int V4l2Codec::startOutput() {
	if (mV4l2Driver->streamOn(OUTPUT_MPLANE))
		return -EINVAL;

	mOutputStreamonDone = true;
	return 0;
}

int V4l2Codec::stopInput() {
	if (mV4l2Driver->streamOff(INPUT_MPLANE))
		return -EINVAL;
	if (mV4l2Driver->isInputRequestEnabled())
		if (mV4l2Driver->closeRequestFd(INPUT_MPLANE))
			return -EINVAL;

	{
		std::unique_lock<std::mutex> lock(mBufLock);
		while (!mPendingInputBufs.empty()) {
			auto buf = mPendingInputBufs.front();
			mInputBufs.push_back(buf);
			mPendingInputBufs.pop_front();
		}
	}
	return 0;
}

int V4l2Codec::stopOutput() {
	if (mV4l2Driver->streamOff(OUTPUT_MPLANE))
		return -EINVAL;

	{
		std::unique_lock<std::mutex> lock(mBufLock);
		while (!mPendingOutputBufs.empty()) {
			auto buf = mPendingOutputBufs.front();
			mOutputBufs.push_back(buf);
			mPendingOutputBufs.pop_front();
		}
	}
	mOutputStreamonDone = false;
	return 0;
}

int V4l2Codec::startMetaInput() {
	if (mV4l2Driver->isInputMetaPortEnabled() &&
		mV4l2Driver->isInputMetadataEnabled())
		if (mV4l2Driver->streamOn(INPUT_META_PLANE))
			return -EINVAL;

	return 0;
}

int V4l2Codec::startMetaOutput() {
	if (mV4l2Driver->isOutputMetaPortEnabled() &&
		mV4l2Driver->isOutputMetadataEnabled())
		if (mV4l2Driver->streamOn(OUTPUT_META_PLANE))
			return -EINVAL;
	return 0;
}

int V4l2Codec::stopMetaInput() {
	if (mV4l2Driver->isInputMetaPortEnabled() &&
		mV4l2Driver->isInputMetadataEnabled())
		if (mV4l2Driver->streamOff(INPUT_META_PLANE))
			return -EINVAL;

	{
		std::unique_lock<std::mutex> lock(mBufLock);
		while (!mPendingMetaInputBufs.empty()) {
			auto buf = mPendingMetaInputBufs.front();
			mMetaInputBufs.push_back(buf);
			mPendingMetaInputBufs.pop_front();
		}
	}
	return 0;
}

int V4l2Codec::stopMetaOutput() {
	if (mV4l2Driver->isOutputMetaPortEnabled() && mV4l2Driver->isOutputMetadataEnabled()) {
		if (mV4l2Driver->streamOff(OUTPUT_META_PLANE)) {
			return -EINVAL;
		}
	}

	{
		std::unique_lock<std::mutex> lock(mBufLock);
		while (!mPendingMetaOutputBufs.empty()) {
			auto buf = mPendingMetaOutputBufs.front();
			mMetaOutputBufs.push_back(buf);
			mPendingMetaOutputBufs.pop_front();
		}
	}
	return 0;
}

int V4l2Codec::getCrop(struct v4l2_rect* crop) {
	crop->left = mCropLeft;
	crop->top = mCropTop;
	crop->width = mCropWidth;
	crop->height = mCropHeight;

	return 0;
}

bool V4l2Codec::isInputMetadata(unsigned int controlId) {
	if (mDomain == V4L2_CODEC_TYPE_DECODER) {
		if (controlId == V4L2_CID_MPEG_VIDC_METADATA_BUFFER_TAG ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_OUTPUT_TX_FENCE ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_PICTURE_TYPE ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_DPB_TAG_LIST) {
			mV4l2Driver->enableInputMetadata(1);
			return true;
		}
	}
	else if (mDomain == V4L2_CODEC_TYPE_ENCODER) {
		if (controlId == V4L2_CID_MPEG_VIDC_METADATA_SEQ_HEADER_NAL ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_EVA_STATS ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_BUFFER_TAG ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_SALIENCY_INFO ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_ROI_INFO ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_VIEW_ID) {
			mV4l2Driver->enableInputMetadata(1);
			return true;
		}
	}
	return false;
}

bool V4l2Codec::isOutputMetadata(unsigned int controlId) {
	if (mDomain == V4L2_CODEC_TYPE_DECODER) {
		if ((controlId == V4L2_CID_MPEG_VIDC_METADATA_BITSTREAM_RESOLUTION ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_CROP_OFFSETS ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_DPB_LUMA_CHROMA_MISR ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_OPB_LUMA_CHROMA_MISR ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_INTERLACE ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_CONCEALED_MB_COUNT ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_HISTOGRAM_INFO ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_SEI_MDCV ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_SEI_CLL ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_BUFFER_TAG ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_SUBFRAME_OUTPUT)) {
			mV4l2Driver->enableOutputMetadata(1);
			return true;
		}
	}
	else if (mDomain == V4L2_CODEC_TYPE_ENCODER) {
		if (controlId == V4L2_CID_MPEG_VIDC_METADATA_LTR_MARK_USE_DETAILS ||
			controlId == V4L2_CID_MPEG_VIDC_METADATA_BUFFER_TAG) {
			mV4l2Driver->enableOutputMetadata(1);
			return true;
		}
	}
	return false;
}

int V4l2Codec::setV4l2Controls() {
	int ret = 0;
	struct v4l2_control control;

	std::list<std::shared_ptr<V4L2ControlInfo>>::iterator it;
	for (std::list<std::shared_ptr<V4L2ControlInfo>>::iterator it =
			mControls.begin(); it != mControls.end(); ++it) {
		isOutputMetadata((*it)->id);
		if (isInputMetadata((*it)->id) || isOutputMetadata((*it)->id)) {
			auto ctrlInfo = std::make_shared<V4L2ControlInfo>();
			ctrlInfo->id = (*it)->id;
			ctrlInfo->value = (*it)->value;
			/* add control to mMetaControls list to use it in fillMetadata */
			mMetaControls.push_back(ctrlInfo);
		}
	}

	while(!mControls.empty()) {
		auto ctrl = mControls.front();
		VIDC_MED("V4l2Codec::setV4l2Controls, id: %#x, value: %d\n", ctrl->id, ctrl->value);

		memset(&control, 0, sizeof(control));
		control.id = ctrl->id;
		control.value = ctrl->value;
		ret = mV4l2Driver->setControl(&control);
		if (ret)
			return ret;

		// check for fence enablement
		if (ctrl->id == V4L2_CID_MPEG_VIDC_METADATA_OUTPUT_TX_FENCE) {
			if (ctrl->value & V4L2_MPEG_VIDC_META_ENABLE &&
				ctrl->value & V4L2_MPEG_VIDC_META_RX_INPUT) {
				mV4l2Driver->enableOutBufFence(1);
				VIDC_MED("V4l2Codec::setV4l2Controls, fence is enabled\n");
			} else {
				mV4l2Driver->enableOutBufFence(0);
				VIDC_MED("V4l2Codec::setV4l2Controls, fence is disabled\n");
			}
		}

		mControls.pop_front();
	}

	return 0;
}

std::shared_ptr<v4l2_buffer> V4l2Codec::allocateBuffer(int index, enum port_type port, int bufSize) {
	VIDC_MED("V4l2Codec::allocateBuffer\n");
	std::shared_ptr<v4l2_buffer> buf;
	struct v4l2_plane* plane;

	if (port != INPUT_PORT && port != OUTPUT_PORT)
	    return nullptr;

	buf = std::make_shared<v4l2_buffer>();
	if (buf == nullptr) {
	    return nullptr;
	}
	memset(buf.get(), 0, sizeof(struct v4l2_buffer));
	plane = (struct v4l2_plane*)malloc(sizeof(struct v4l2_plane) * VIDEO_MAX_PLANES);
	if (plane == nullptr) {
	    return nullptr;
	}
	memset(plane, 0, sizeof(struct v4l2_plane) * VIDEO_MAX_PLANES);

	buf->index = index;
	buf->type = port == INPUT_PORT ? INPUT_MPLANE :
		OUTPUT_MPLANE;
	buf->flags = 0;
	buf->memory = V4L2_MEMORY_DMABUF;
	buf->length = 1;
	memset(&buf->timestamp, 0, sizeof(buf->timestamp));
	buf->m.planes = plane;
	plane[0].bytesused = 0;
	plane[0].length = bufSize * (port == INPUT_PORT ? mMultiplier : 1);
	plane[0].m.fd = mV4l2Driver->ionAlloc(plane[0].length);
	if (plane[0].m.fd < 0)
	    return nullptr;
	plane[0].data_offset = 0;
	return buf;
}

std::shared_ptr<v4l2_buffer> V4l2Codec::allocateMetaBuffer(int index, enum port_type port, int bufSize) {
	std::shared_ptr<v4l2_buffer> buf;

	if (port != INPUT_META_PORT && port != OUTPUT_META_PORT)
	    return nullptr;

	if (!bufSize)
		return nullptr;

	buf = std::make_shared<v4l2_buffer>();
	if (buf == nullptr) {
	    return nullptr;
	}
	memset(buf.get(), 0, sizeof(struct v4l2_buffer));

	buf->index = index;
	buf->type = port == INPUT_META_PORT ? INPUT_META_PLANE : OUTPUT_META_PLANE;
	buf->flags = 0;
	buf->memory = V4L2_MEMORY_DMABUF;
	buf->length = bufSize;
	buf->bytesused = 100;
	buf->m.fd = mV4l2Driver->ionAlloc(bufSize);
	if (buf->m.fd < 0)
		return nullptr;

	return buf;
}

int V4l2Codec::allocateBuffers(enum port_type port) {
	VIDC_MED("V4l2Codec::allocateBuffers, enter\n");
	int bufCount = 0, bufSize = 0;
	std::shared_ptr<v4l2_buffer> buf;

	if (port != INPUT_PORT && port != OUTPUT_PORT)
		return -EINVAL;

	if (port == INPUT_PORT) {
		bufCount = getMinInputCount();
		bufSize = getInputSize();
		if (mDomain == V4L2_CODEC_TYPE_DECODER) {
			if (bufSize < mInputSizeOverWrite)
				bufSize = mInputSizeOverWrite;
		}
	} else {
		bufCount = getOutputAllocCount();
		bufSize = getOutputSize();
	}

	for (int i = 0; i < bufCount; i++) {
		buf = allocateBuffer(i, port, bufSize);
		if (buf == nullptr)
			return -EINVAL;

		{
			std::unique_lock<std::mutex> lock(mBufLock);
			if (port == INPUT_PORT)
				mInputBufs.push_back(buf);
			else
				mOutputBufs.push_back(buf);
		}
		print_v4l2_buffer("AllocBuffer", buf.get());
	}
	VIDC_MED("V4l2Codec::allocateBuffers, exit\n");

	return 0;
}

int V4l2Codec::allocateBuffersSingleFd(enum port_type port) {
	int bufCount = 0, bufSize = 0;
	int fd, offset = 0;
	std::shared_ptr<v4l2_buffer> buf;
	struct v4l2_plane* plane;

	if (port != INPUT_PORT || mDomain != V4L2_CODEC_TYPE_DECODER)
		return -EINVAL;

	bufCount = getMinInputCount();
	bufSize = getInputSize();
	if (bufSize < mInputSizeOverWrite)
		bufSize = mInputSizeOverWrite;
	bufSize = ALIGN(bufSize, 4096);

	if (bufCount <= 0)
		return -EINVAL;

	fd = mV4l2Driver->ionAlloc(bufSize * bufCount);
	if (fd < 0)
		return -EINVAL;

	for (int i = 0; i < bufCount; i++) {
		buf = std::make_shared<v4l2_buffer>();
		if (buf == nullptr) {
		    return -EINVAL;
		}
		memset(buf.get(), 0, sizeof(struct v4l2_buffer));
		plane = (struct v4l2_plane*)malloc(sizeof(struct v4l2_plane) * VIDEO_MAX_PLANES);
		if (plane == nullptr) {
		    return -EINVAL;
		}
		memset(plane, 0, sizeof(struct v4l2_plane) * VIDEO_MAX_PLANES);

		buf->index = i;
		buf->type = INPUT_MPLANE;
		buf->flags = 0;
		buf->memory = V4L2_MEMORY_DMABUF;
		buf->length = 1;
		memset(&buf->timestamp, 0, sizeof(buf->timestamp));
		buf->m.planes = plane;
		plane[0].bytesused = 0;
		plane[0].length = bufSize * (i + 1);
		plane[0].m.fd = fd;
		plane[0].data_offset = bufSize * i;

		{
			std::unique_lock<std::mutex> lock(mBufLock);
			mInputBufs.push_back(buf);
		}
		print_v4l2_buffer("AllocBuffer", buf.get());
	}

	return 0;
}

int V4l2Codec::allocateMetaBuffers(enum port_type port) {
	int bufCount = 0, bufSize = 0;
	std::shared_ptr<v4l2_buffer> buf;

	if (port != INPUT_META_PORT && port != OUTPUT_META_PORT)
		return -EINVAL;

	if (port == INPUT_META_PORT) {
		bufCount = getMinInputCount();
		bufSize = getMetaInputSize();
	}
	else {
		bufCount = getMinOutputCount();
		bufSize = getMetaOutputSize();
	}

	if (port == INPUT_META_PORT) {
		if (!isInputMetadataEnabled()) {
			return 0;
		}
	}
	else if (port == OUTPUT_META_PORT) {
		if (!isOutputMetadataEnabled()) {
			return 0;
		}
	}

	if (!bufSize) {
		VIDC_ERR("V4l2Codec::allocateMetaBuffers, zero length metabuffer size of type %d not allowed\n", port);
		return -EINVAL;
	}

	for (int i = 0; i < bufCount; i++) {
		buf = allocateMetaBuffer(i, port, bufSize);
		if (buf == nullptr)
			return -EINVAL;

		{
			std::unique_lock<std::mutex> lock(mBufLock);
			if (port == INPUT_META_PORT)
				mMetaInputBufs.push_back(buf);
			else
				mMetaOutputBufs.push_back(buf);
		}
		print_v4l2_buffer("AllocMetaBuffer", buf.get());
	}

	return 0;
}

void V4l2Codec::freeMetaBuffers(enum port_type port) {
	if (port == OUTPUT_META_PORT) {
		std::unique_lock<std::mutex> lock(mBufLock);
		while(!mMetaOutputBufs.empty()) {
			auto metaBuf = mMetaOutputBufs.front();
			print_v4l2_buffer("FreeMetaBuffer", metaBuf.get());
			mV4l2Driver->ionFree(metaBuf->m.fd);
			mMetaOutputBufs.pop_front();
		}
	} else if (port == INPUT_META_PORT) {
		std::unique_lock<std::mutex> lock(mBufLock);
		while (!mMetaInputBufs.empty()) {
			auto metaBuf = mMetaInputBufs.front();
			print_v4l2_buffer("FreeMetaBuffer", metaBuf.get());
			mV4l2Driver->ionFree(metaBuf->m.fd);
			mMetaInputBufs.pop_front();
		}
	}
}

void V4l2Codec::freeBuffers(enum port_type port) {
	if (port == OUTPUT_PORT) {
		std::unique_lock<std::mutex> lock(mBufLock);
		while (!mOutputBufs.empty()) {
			auto buf = mOutputBufs.front();
			print_v4l2_buffer("FreeBuffer", buf.get());
			mV4l2Driver->ionFree(buf->m.planes[0].m.fd);
			free(buf->m.planes);
			mOutputBufs.pop_front();
		}
		while (!mPendingOutputBufs.empty()) {
			auto buf = mPendingOutputBufs.front();
			print_v4l2_buffer("FreeBuffer", buf.get());
			mV4l2Driver->ionFree(buf->m.planes[0].m.fd);
			free(buf->m.planes);
			mPendingOutputBufs.pop_front();
		}
	} else if (port == INPUT_PORT) {
		std::unique_lock<std::mutex> lock(mBufLock);
		while (!mInputBufs.empty()) {
			auto buf = mInputBufs.front();
			print_v4l2_buffer("FreeBuffer", buf.get());
			mV4l2Driver->ionFree(buf->m.planes[0].m.fd);
			free(buf->m.planes);
			mInputBufs.pop_front();
		}
		while (!mPendingInputBufs.empty()) {
			auto buf = mPendingInputBufs.front();
			print_v4l2_buffer("FreeBuffer", buf.get());
			mV4l2Driver->ionFree(buf->m.planes[0].m.fd);
			free(buf->m.planes);
			mPendingInputBufs.pop_front();
		}
	}
}

void V4l2Codec::freeBuffersSingleFd(enum port_type port) {
	if (port == INPUT_PORT) {
		std::unique_lock<std::mutex> lock(mBufLock);
		while (!mInputBufs.empty()) {
			auto buf = mInputBufs.front();
			print_v4l2_buffer("FreeBuffer", buf.get());
			if (!buf->m.planes[0].data_offset)
				mV4l2Driver->ionFree(buf->m.planes[0].m.fd);
			free(buf->m.planes);
			mInputBufs.pop_front();
		}
		while (!mPendingInputBufs.empty()) {
			auto buf = mPendingInputBufs.front();
			print_v4l2_buffer("FreeBuffer", buf.get());
			if (!buf->m.planes[0].data_offset)
				mV4l2Driver->ionFree(buf->m.planes[0].m.fd);
			free(buf->m.planes);
			mPendingInputBufs.pop_front();
		}
	}
}

int V4l2Codec::ionAlloc(int size) {
	return mV4l2Driver->ionAlloc(size);
}

void V4l2Codec::ionFree(int fd) {
	mV4l2Driver->ionFree(fd);
}

int V4l2Codec::setControl(unsigned int ctrlId, int value) {
	int ret = 0;
	struct v4l2_control control;

	memset(&control, 0, sizeof(control));
	control.id = ctrlId;
	control.value = value;
	ret = mV4l2Driver->setControl(&control);

	return ret;
}

bool V4l2Codec::isAPVSupported() {
	struct v4l2_format fmt;
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = INPUT_MPLANE;
	if (mV4l2Driver->getFormat(&fmt))
		return false;
	if (fmt.fmt.pix_mp.pixelformat != V4L2_PIX_FMT_VIDC_APV)
		return false;
	return true;
}

int V4l2Codec::queryControl(struct v4l2_queryctrl *queryctrl) {
	int ret = 0;

	if (!queryctrl) {
		VIDC_ERR("V4l2Codec::queryControl, Invalid param\n");
		return -EINVAL;
	}

	ret = mV4l2Driver->queryControl(queryctrl);

	if (ret) {
		VIDC_ERR("V4l2Codec::queryControl, queryControl failed\n");
		return ret;
	}

	return 0;
}

int V4l2Codec::queryMenu(struct v4l2_querymenu *querymenu) {
	int ret = 0;

	if (!querymenu) {
		VIDC_ERR("V4l2Codec::queryMenu, Invalid param\n");
		return -EINVAL;
	}

	ret = mV4l2Driver->queryMenu(querymenu);
	if (ret) {
		VIDC_ERR("V4l2Codec::queryMenu, queryMenu failed\n");
		return ret;
	}

	return 0;
}

void V4l2Codec::setEarlyNotifyIntrptCount(uint32_t count) {
	mV4l2Driver->setEarlyNotifyIntrptCount(count);
}

int V4l2Codec::extractMetadata(struct v4l2_buffer* metaBuf, struct V4L2OutputFenceInfo* fenceInfo)
{
	if (metaBuf == nullptr)
		return -EINVAL;
	if (metaBuf->m.fd < 0)
		return -ENOMEM;
	struct msm_vidc_metabuf_header* mhdr;
#if defined (ANDROID) || defined (_LINUX_VENV_)
	mhdr = reinterpret_cast<struct msm_vidc_metabuf_header*>(mmap
		(0, metaBuf->length, PROT_READ| PROT_WRITE, MAP_SHARED, metaBuf->m.fd, 0));
#else
	return 0;
#endif
	if (mhdr == nullptr) {
		VIDC_ERR("V4l2Codec::extractMetadata, invalid meta hdeader\n");
		return EINVAL;
	}
	uint32_t pldcnt = 0;
	struct msm_vidc_metapayload_header* mphdr = reinterpret_cast<struct msm_vidc_metapayload_header*>(mhdr + 1);

	while (pldcnt != mhdr->count) {
		if (mphdr == nullptr) {
			VIDC_ERR("V4l2Codec::extractMetadata, invalid meta payload hdeader\n");
			return -EINVAL;
		}
		switch (mphdr->type) {
			case METADATA_BUFFER_TAG:
			{
				uint64_t *tagptr = reinterpret_cast<uint64_t *>
					((reinterpret_cast<uintptr_t>(mhdr) + mphdr->offset));
				if (!tagptr) {
					VIDC_ERR("V4l2Codec::extractMetadata, invalid tagdata");
					return -EINVAL;
				}
				if (metaBuf->type == INPUT_META_PLANE && mV4l2Driver->isOutBufFenceEnabled() &&
					fenceInfo != nullptr) {
					fenceInfo->outputBufTag = *tagptr;
				}
				VIDC_MED("V4l2Codec::extractMetadata, index %u %s %llu\n", metaBuf->index,
					metaBuf->type == INPUT_META_PLANE ? "Input buffer's Output tag" :
					metaBuf->type == OUTPUT_META_PLANE ? "Output buffer's Input tag" :
					"UNKNOWN tag", *tagptr);
				break;
			}
			case METADATA_SUBFRAME_OUTPUT:
			{
				uint32_t *subframe = reinterpret_cast<uint32_t *>
					((reinterpret_cast<uintptr_t>(mhdr) + mphdr->offset));
				if (!subframe) {
					VIDC_ERR("V4l2Codec::extractMetadata, invalid subframe data");
					return -EINVAL;
				}
				setCompleteFrame(*subframe ? false : true);
				VIDC_MED("SV4l2Codec::extractMetadata, ubframe flag %s\n", *subframe ? "Set" : "Not-Set");
				break;
			}
			case METADATA_DPB_LUMA_CHROMA_MISR:
			{
				uint32_t *base = reinterpret_cast<uint32_t *>
					((reinterpret_cast<uintptr_t>(mhdr) + mphdr->offset));
				if (!base) {
					VIDC_ERR("V4l2Codec::extractMetadata, invalid MISR info \n");
					return -EINVAL;
				}

				//TODO: Check MISR based on interlace type
				for (int i = 0; i < kNumPipes && mMisrFrame < MAX_FRAMES; i++) {
					if (base[i * 2] != kMisrArr[mMisrFrame][i * 2]) {
						VIDC_MED("V4l2Codec::extractMetadata, Luma DPB MISR mismatch at %d frame %d \n", i * 2, mMisrFrame);
						break;
					}
					if (base[i * 2 + 1] != kMisrArr[mMisrFrame][i * 2 + 1]) {
						VIDC_MED("V4l2Codec::extractMetadata, Chroma DPB MISR mismatch at %d frame %d \n", i * 2 + 1, mMisrFrame);
						break;
					}
				}
				mMisrFrame++;
				break;
			}
			case METADATA_OPB_LUMA_CHROMA_MISR:
			{
				//TODO: Check MISR based on interlace type
				break;
			}
			case METADATA_INTERLACE_INFO:
			{
				enum meta_interlace_info *base = reinterpret_cast<enum meta_interlace_info *>
					((reinterpret_cast<uintptr_t>(mhdr) + mphdr->offset));

				if (!base) {
					VIDC_ERR("V4l2Codec::extractMetadata, invalid interlace info \n");
					return -EINVAL;
				}
				VIDC_ERR("V4l2Codec::extractMetadata, interlace type: %d\n", *base);
				break;
			}
			case METADATA_FENCE_OUTPUT:
			{
				uint32_t *fence = reinterpret_cast<uint32_t *>
					((reinterpret_cast<uintptr_t>(mhdr) + mphdr->offset));
				if (!fence) {
					VIDC_ERR("V4l2Codec::extractMetadata, invalid fence data");
					return -EINVAL;
				}
				if (metaBuf->type == INPUT_META_PLANE && mV4l2Driver->isOutBufFenceEnabled() &&
					fenceInfo != nullptr)
					fenceInfo->fenceIds.push_back(*fence);
				VIDC_MED("V4l2Codec::extractMetadata, Fence id %u\n", *fence);
				break;
			}
			default:
			{
				VIDC_ERR("V4l2Codec::extractMetadata, Received metadata: 0x%x\n", mphdr->type);
			}
		}
		pldcnt++;
		mphdr++;
	}
	return 0;
}

int V4l2Codec::queueBufferRequest(std::shared_ptr<v4l2_buffer> buffer, unsigned int currentFrameNum) {
	int ret = 0;
	struct v4l2_ext_controls ext_controls;
	memset(&ext_controls, 0, sizeof(ext_controls));
	ext_controls.controls = (struct v4l2_ext_control *)malloc(sizeof(struct v4l2_ext_control) * MAX_EXT_CONTROLS);
	if (!ext_controls.controls) {
		return -EINVAL;
	}

	if (mDynamicParams.empty())
		goto queuebufferLabel;

	for (auto itr = mDynamicParams.begin(); itr != mDynamicParams.end(); ) {
		auto param = *itr;
		if (param->frameNum != currentFrameNum) {
			itr++;
			continue;
		}
		if (param->controlId == SPARM_FRAME_RATE) {
			ret = setFrameRate(1, param->value);
			if (ret)
				return ret;
			itr = mDynamicParams.erase(itr);
		}
		else if (param->controlId == SPARM_OPERATING_RATE) {
			ret = setOperatingRate(1, param->value);
			if (ret)
				return ret;
			itr = mDynamicParams.erase(itr);
		}
		else if (param->controlId == V4L2_CID_MPEG_VIDEO_FORCE_KEY_FRAME) {
			/* button controls are not supported via request api*/
			ret = setControl(param->controlId, param->value);
			if (ret)
				return ret;
			itr = mDynamicParams.erase(itr);
		}
		else {
			ext_controls.controls[ext_controls.count].id = param->controlId;
			ext_controls.controls[ext_controls.count].value = param->value;
			ext_controls.count++;
			if (ext_controls.count >= MAX_EXT_CONTROLS) {
				VIDC_ERR("V4l2Codec::queueBufferRequest, dynamic controls count %d exceeds max allowed %d\n",
					ext_controls.count, MAX_EXT_CONTROLS);
				return -EINVAL;
			}
			itr = mDynamicParams.erase(itr);
			itr++;
		}
	}

	if (ext_controls.count <= 0)
		goto queuebufferLabel;

	ret = mV4l2Driver->queueBufferRequest(buffer.get(), &ext_controls);
	if (ret)
		return ret;
	free(ext_controls.controls);

	return ret;

queuebufferLabel:
	ret = mV4l2Driver->queueBufferRequest(buffer.get(), NULL);
	if (ret)
		return ret;
	free(ext_controls.controls);
	return ret;
}

int V4l2Codec::queueBuffer(std::shared_ptr<v4l2_buffer> buffer) {
	VIDC_MED("V4l2Codec: queueBuffer, buffer byteused = %d\n", buffer->m.planes[0].bytesused);
	if (buffer->type == INPUT_META_PLANE) {
		/* queue input metabuffer via meta port if input meta port enabled */
		if (mV4l2Driver->isInputMetaPortEnabled()) {
			return mV4l2Driver->queueBuf(buffer.get());
		} else {
			/* neither meta port nor request enabled */
			return -EINVAL;
		}
	}

	/* queue input buffer with request when mInputRequestEnabled is enabled */
	if (mV4l2Driver->isInputRequestEnabled() && buffer->type == INPUT_MPLANE)
		return queueBufferRequest(buffer, mFrameCount);

	return queueBuffer(buffer, mFrameCount);
}

int V4l2Codec::queueBuffer(std::shared_ptr<v4l2_buffer> buffer, unsigned int currentFrameNum)
{
	int ret = 0;

	if (buffer->type != INPUT_MPLANE)
		goto queuebufferLabel;

	if (mDynamicParams.empty())
		goto queuebufferLabel;

	for (auto itr = mDynamicParams.begin(); itr != mDynamicParams.end(); ) {
		auto param = *itr;
		if (param->frameNum != currentFrameNum) {
			itr++;
			continue;
		}
		if (param->controlId == SPARM_FRAME_RATE) {
			ret = setFrameRate(1, param->value);
			if (ret)
				return ret;
			itr = mDynamicParams.erase(itr);
		} else if (param->controlId == SPARM_OPERATING_RATE) {
			ret = setOperatingRate(1, param->value);
			if (ret)
				return ret;
			itr = mDynamicParams.erase(itr);
		} else {
			ret = setControl(param->controlId, param->value);
			if (ret)
				return ret;
			itr = mDynamicParams.erase(itr);
		}
	}

queuebufferLabel:
	ret = mV4l2Driver->queueBuf(buffer.get());
	if (ret) {
		return ret;
	}

	return ret;
}

int V4l2Codec::fillMetadata(std::shared_ptr<v4l2_buffer> metaBuf) {
	if (metaBuf == nullptr)
		return -EINVAL;
	if (metaBuf->m.fd < 0)
		return -ENOMEM;

	if (metaBuf->type != INPUT_META_PLANE && metaBuf->type != OUTPUT_META_PLANE) {
		VIDC_ERR("V4l2Codec::fillMetadata, invalid buffer type %u", metaBuf->type);
		return -EINVAL;
	}
	struct msm_vidc_metabuf_header* mhdr;
#if defined (ANDROID) || defined (_LINUX_VENV_)
	mhdr = reinterpret_cast<struct msm_vidc_metabuf_header*>(mmap
		(0, metaBuf->length, PROT_READ| PROT_WRITE, MAP_SHARED, metaBuf->m.fd, 0));
#else
	mhdr = reinterpret_cast<struct msm_vidc_metabuf_header*>(metaBuf->m.fd);
#endif
	memset(mhdr, 0, sizeof(struct msm_vidc_metabuf_header));
	mhdr->size = sizeof(struct msm_vidc_metabuf_header);
	mhdr->version = 1 << 16;

	struct msm_vidc_metapayload_header* mphdr = reinterpret_cast<struct msm_vidc_metapayload_header*>(mhdr + 1);
	uint32_t metaPayloadOffset = sizeof(struct msm_vidc_metabuf_header) +
					sizeof(struct msm_vidc_metapayload_header) * MAX_META_DELIVERY_PAYLOADS;

	for (auto ctrl = mMetaControls.begin(); ctrl != mMetaControls.end(); ++ctrl) {
		auto control = *ctrl;
		if (!control->value)
			continue;

		switch (control->id) {
			case V4L2_CID_MPEG_VIDC_METADATA_BUFFER_TAG:
			{
				memset(mphdr, 0, sizeof(struct msm_vidc_metapayload_header));
				mphdr->type = METADATA_BUFFER_TAG;
				mphdr->size = sizeof(uint64_t);
				mphdr->version = 1 << 16;
				mphdr->offset = metaPayloadOffset;
				mphdr->flags = METADATA_FLAGS_NONE;

				uint64_t* tagptr = reinterpret_cast<uint64_t *>
					((reinterpret_cast<uintptr_t>(mhdr) + mphdr->offset));
				if (!tagptr) {
					VIDC_ERR("V4l2Codec::fillMetadata, tagptr is null");
					return -ENOMEM;
				}

				if (metaBuf->type == INPUT_META_PLANE) {
					*tagptr = InputTag::fromId(metaBuf->index);
					VIDC_MED("V4l2Codec::fillMetadata, Input Tag %llu\n", *tagptr);
				} else {
					*tagptr = OutputTag::fromId(metaBuf->index);
					VIDC_MED("V4l2Codec::fillMetadata, Output Tag %llu\n", *tagptr);
				}

				metaPayloadOffset += mphdr->size;
				mhdr->size += sizeof(struct msm_vidc_metapayload_header) + mphdr->size;
				mhdr->count++;
				mphdr++;
				break;
			}
			case V4L2_CID_MPEG_VIDC_METADATA_EVA_STATS:
			{
				memset(mphdr, 0, sizeof(struct msm_vidc_metapayload_header));
				mphdr->type = METADATA_EVA_STAT_INFO;
				mphdr->size = CVP_METADATA_SIZE;
				mphdr->version = 1 << 16;
				mphdr->offset = metaPayloadOffset;
				mphdr->flags = METADATA_FLAGS_NONE;
				uint8_t *cvpMeta = reinterpret_cast<uint8_t *>
					((reinterpret_cast<uintptr_t>(mhdr) + mphdr->offset));

				if (!mCvpMetaFile) {
					VIDC_ERR("V4l2Codec::fillMetadata, Missing CVP Meta File \n");
					return -EINVAL;
				}
				if (fread(cvpMeta, 1024, 1, mCvpMetaFile) != 1) {
					VIDC_ERR("V4l2Codec::fillMetadata, Failed to read CVP Meta File \n");
					return -EINVAL;
				}

				uint32_t *captureFrameRate = reinterpret_cast<uint32_t *>(&cvpMeta[1024]);
				uint32_t *evaFrameRate = captureFrameRate + 1;
				uint32_t *evaMetaFlags = evaFrameRate + 1;
				/* Use the default 30 fps (Q16 format) */
				*captureFrameRate = 1966080;
				*evaFrameRate = 1966080;
				*evaMetaFlags = 0;

				metaPayloadOffset += mphdr->size;
				mhdr->size += sizeof(struct msm_vidc_metapayload_header) + mphdr->size;
				mhdr->count++;
				mphdr++;
				break;
			}
			case V4L2_CID_MPEG_VIDC_METADATA_SALIENCY_INFO:
			{
				memset(mphdr, 0, sizeof(struct msm_vidc_metapayload_header));
				mphdr->type = METADATA_ROI_AS_SALIENCY_INFO;
				mphdr->size = SALIENCY_METADATA_SIZE;
				mphdr->version = 1 << 16;
				mphdr->offset = metaPayloadOffset;
				mphdr->flags = METADATA_FLAGS_NONE;

				uint32_t *saliencyROI = reinterpret_cast<uint32_t *>
					((reinterpret_cast<uintptr_t>(mhdr) + mphdr->offset));
				saliencyROI[0] = METADATA_SALIENCY_TYPE0;
				saliencyROI[1] = (mWidth) << 16 | (mHeight);
				saliencyROI[2] = (mWidth) << 16 | (mHeight);
				saliencyROI[3] = (mWidth) << 16 | (mHeight);

				metaPayloadOffset += mphdr->size;
				mhdr->size += sizeof(struct msm_vidc_metapayload_header) + mphdr->size;
				mhdr->count++;
				mphdr++;
				break;
			}
			case V4L2_CID_MPEG_VIDC_METADATA_ROI_INFO:
			{
				uint16_t *pBuf;
				uint32_t lcuWidth, lcuHeight, rowSize, bufSize;

				if (mCodec == V4L2_PIX_FMT_HEVC) {
					lcuWidth = (mWidth + 31) >> 5;
					lcuHeight = (mHeight + 31) >> 5;
				} else {
					lcuWidth = (mWidth + 15) >> 4;
					lcuHeight = (mHeight + 15) >> 4;
				}
				rowSize = (((lcuWidth + 7) >> 3) << 3);
				bufSize = rowSize * lcuHeight * 2 + 256;

				memset(mphdr, 0, sizeof(struct msm_vidc_metapayload_header));
				mphdr->type = METADATA_ROI_INFO;
				mphdr->size = bufSize;
				mphdr->version = 1 << 16;
				mphdr->offset = ALIGN(metaPayloadOffset, (uint32_t)256);
				mphdr->flags = METADATA_FLAGS_NONE;

				uint16_t *pExtraDataBuf = reinterpret_cast<uint16_t *>
						((reinterpret_cast<uintptr_t>(mhdr) + mphdr->offset));

				for (int j = 0; j < lcuHeight; j++) {
					pBuf = pExtraDataBuf + j * rowSize;
					for (int i = 0; i < lcuWidth/2; i++) {
						uint8_t temp = 6;
						*pBuf++ = (1 << 11) | ((temp & 0x3F) << 4);
					}
					for (int i = lcuWidth/2; i < lcuWidth; i++) {
						uint8_t temp = -6;
						*pBuf++ = (1 << 11) | ((temp & 0x3F) << 4);
					}
				}

				uint32_t payloadOffset = ALIGN(metaPayloadOffset, (uint32_t)256) - metaPayloadOffset;
				metaPayloadOffset += ALIGN(mphdr->size + payloadOffset, (uint32_t)4);
				mhdr->size += sizeof(struct msm_vidc_metapayload_header) + ALIGN(mphdr->size + payloadOffset, (uint32_t)4);
				mhdr->count++;
				mphdr++;
				break;
			}
			case V4L2_CID_MPEG_VIDC_METADATA_VIEW_ID:
			{
				memset(mphdr, 0, sizeof(struct msm_vidc_metapayload_header));
				mphdr->type = METADATA_MULTI_VIEW;
				mphdr->size = sizeof(uint64_t);
				mphdr->version = 1 << 16;
				mphdr->offset = metaPayloadOffset;
				mphdr->flags = METADATA_FLAGS_NONE;

				uint64_t *viewIdMeta = reinterpret_cast<uint64_t *>
					((reinterpret_cast<uintptr_t>(mhdr) + mphdr->offset));
				static bool viewId = true;
				*viewIdMeta = viewId ? 0 : 1;
				viewId = !viewId;
				VIDC_MED("V4l2Codec::fillMetadata, View id is %d \n", *viewIdMeta);

				metaPayloadOffset += mphdr->size;
				mhdr->size += sizeof(struct msm_vidc_metapayload_header) + mphdr->size;
				mhdr->count++;
				mphdr++;
				break;
			}
			default:
			{
				break;
			}
		}
	}
	metaBuf->bytesused = metaPayloadOffset;

	return 0;
}


