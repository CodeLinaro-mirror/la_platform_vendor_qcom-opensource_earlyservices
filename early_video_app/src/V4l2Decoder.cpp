/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include "../inc/V4l2Decoder.h"
#include "../inc/V4l2Codec.h"
#include "../inc/VideoDefines.h"
#include "../inc/V4l2Callback.h"
#include "../inc/VidcLog.h"


using namespace early_video_app;

#define MAX_COLOR_FMTS 7

int V4l2Decoder::init(unsigned int codec) {
	struct v4l2_format fmt;
	struct v4l2_control ctrl;
	struct v4l2_selection sel;
	struct v4l2_capability caps;

	VIDC_MED("V4l2Decoder::init\n");

	mCodec = codec;
	mDomain = V4L2_CODEC_TYPE_DECODER;

	VIDC_MED("V4l2Decoder::init, open decoder\n");
	if (mV4l2Driver->Open(V4L2_CODEC_TYPE_DECODER)) {
		VIDC_ERR("V4l2Decoder::init, open decoder failed\n");
		return -EINVAL;
	}

	VIDC_MED("V4l2Decoder::init, subscribe source change event\n");
	if (mV4l2Driver->subscribeEvent(V4L2_EVENT_SOURCE_CHANGE)) {
		VIDC_ERR("V4l2Decoder::init, subscribe source change event failed\n");
		return -EINVAL;
	}

	VIDC_MED("V4l2Decoder::init, subscribe EOS event\n");
	if (mV4l2Driver->subscribeEvent(V4L2_EVENT_EOS)) {
		VIDC_ERR("V4l2Decoder::init, subscribe EOS event failed\n");
		return -EINVAL;
	}

	VIDC_MED("V4l2Decoder::init, create poll thread\n");
	if (mV4l2Driver->createPollThread()) {
		VIDC_ERR("V4l2Decoder::init, create poll thread failed\n");
		return -EINVAL;
	}

	memset(&caps, 0, sizeof(caps));
	VIDC_MED("V4l2Decoder::init, query capabilities\n");
	if (mV4l2Driver->queryCapabilities(&caps)) {
		VIDC_ERR("V4l2Decoder::init, query capabilities failed\n");
		return -EINVAL;
	}
	VIDC_MED("V4l2Decoder::init, enable input meta port\n");
	if (caps.device_caps & V4L2_CAP_META_OUTPUT) {
		mV4l2Driver->enableInputMetaPort(1);
	}
	/* enable input meta port to send input meta buffers */
	mV4l2Driver->enableInputMetaPort(1);
	VIDC_MED("V4l2Decoder::init, enable output meta port\n");
	if (caps.device_caps & V4L2_CAP_META_CAPTURE) {
		mV4l2Driver->enableOutputMetaPort(1);
	}

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = INPUT_MPLANE;
	VIDC_MED("V4l2Decoder::init, get input format\n");
	if (mV4l2Driver->getFormat(&fmt)) {
		VIDC_ERR("V4l2Decoder::init, get input format failed\n");
		return -EINVAL;
	}
	fmt.fmt.pix_mp.pixelformat = mCodec;
	VIDC_MED("V4l2Decoder::init, set input format\n");
	if (mV4l2Driver->setFormat(&fmt)) {
		VIDC_ERR("V4l2Decoder::init, set input format failed\n");
		return -EINVAL;
	}

	mWidth = fmt.fmt.pix_mp.width;
	mHeight = fmt.fmt.pix_mp.height;
	mStride = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
	mScanline = fmt.fmt.pix_mp.height;
	mInputSize = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

	memset(&mOutputFormat, 0, sizeof(mOutputFormat));
	mOutputFormat.type = OUTPUT_MPLANE;
	VIDC_MED("V4l2Decoder::init, get output format\n");
	if (mV4l2Driver->getFormat(&mOutputFormat)) {
		VIDC_ERR("V4l2Decoder::init, get output format failed\n");
		return -EINVAL;
	}
	mColorFormat = mOutputFormat.fmt.pix_mp.pixelformat;
	mOutputColorPrimaries = mOutputFormat.fmt.pix_mp.colorspace;
	mOutputMatrixCoeff = mOutputFormat.fmt.pix_mp.ycbcr_enc;
	mOutputTransferChar = mOutputFormat.fmt.pix_mp.xfer_func;
	mOutputVideoRange = mOutputFormat.fmt.pix_mp.quantization;
	mOutputSize = mOutputFormat.fmt.pix_mp.plane_fmt[0].sizeimage;

	memset(&sel, 0, sizeof(sel));
	sel.type = OUTPUT_MPLANE;
	sel.target = V4L2_SEL_TGT_COMPOSE;
	VIDC_MED("V4l2Decoder::init, get output selection\n");
	if (mV4l2Driver->getSelection(&sel)) {
		VIDC_ERR("V4l2Decoder::init, get output selection failed\n");
		return -EINVAL;
	}
	mCropLeft = sel.r.left;
	mCropTop = sel.r.top;
	mCropWidth = sel.r.width;
	mCropHeight = sel.r.height;

	ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_OUTPUT;
	VIDC_MED("V4l2Decoder::init, get output min buffers control\n");
	if (mV4l2Driver->getControl(&ctrl)) {
		VIDC_ERR("V4l2Decoder::init, get output min buffers control failed\n");
		return -EINVAL;
	}
	mMinInputCount = ctrl.value;
	mActualInputCount = mMinInputCount;

	ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
	VIDC_MED("V4l2Decoder::init, get capture min buffers control\n");
	if (mV4l2Driver->getControl(&ctrl)) {
		VIDC_ERR("V4l2Decoder::init, get capture min buffers control failed\n");
		return -EINVAL;
	}
	mMinOutputCount = ctrl.value;
	mActualOutputCount = mMinOutputCount + 4;

	return 0;
}

void V4l2Decoder::deinit() {
	mV4l2Driver->stopPollThread();
	mV4l2Driver->unsubscribeEvent(V4L2_EVENT_EOS);
	mV4l2Driver->unsubscribeEvent(V4L2_EVENT_SOURCE_CHANGE);
	mV4l2Driver->Close();
	mV4l2Driver->closeMediaDevice();
	memset(&mOutputFormat, 0, sizeof(mOutputFormat));
	mFrameRate = 0;
	VIDC_MED("V4l2Decoder::deinit\n");
}

int V4l2Decoder::setOperatingRate(unsigned int numer, unsigned int denom) {
	struct v4l2_control control;
	memset(&control, 0, sizeof(control));

	control.id = V4L2_CID_MPEG_VIDC_OPERATING_RATE;
	control.value = (denom / numer) << 16;
	VIDC_MED("V4l2Decoder::setOperatingRate, setopRate id %d val %d", control.id, control.value);
	if (mV4l2Driver->setControl(&control)) {
		VIDC_ERR("V4l2Decoder::setOperatingRate, set control failed\n");
		return -EINVAL;
	}

	return 0;
}

int V4l2Decoder::setFrameRate(unsigned int numer, unsigned int denom) {
	struct v4l2_control control;
	memset(&control, 0, sizeof(control));

	control.id = V4L2_CID_MPEG_VIDC_FRAME_RATE;
	control.value = (denom / numer) << 16;
	VIDC_MED("V4l2Decoder::setFrameRate, setFrameRate id %d val %d", control.id, control.value);
	if (mV4l2Driver->setControl(&control)) {
		VIDC_ERR("V4l2Decoder::setFrameRate, set control failed\n");
		return -EINVAL;
	}

	return 0;
}

int V4l2Decoder::getOperatingRate() {
	struct v4l2_control control;
	memset(&control, 0, sizeof(control));

	control.id = V4L2_CID_MPEG_VIDC_OPERATING_RATE;
	if (mV4l2Driver->getControl(&control)) {
		VIDC_ERR("V4l2Decoder::getOperatingRate, set control failed\n");
		return -EINVAL;
	}

	return 0;
}

int V4l2Decoder::setDSResolution(unsigned int width, unsigned int height) {
	struct v4l2_selection sel;
	memset(&sel, 0, sizeof(sel));

	sel.type = OUTPUT_MPLANE;
	sel.target = V4L2_SEL_TGT_COMPOSE;
	sel.r.width = width;
	sel.r.height = height;
	return mV4l2Driver->setSelection(&sel);
}

float V4l2Decoder::getFrameRate() {
	if (mFrameRate > 0) {
		return mFrameRate;
	}

	struct v4l2_control control;
	memset(&control, 0, sizeof(control));

	control.id = V4L2_CID_MPEG_VIDC_FRAME_RATE;
	if (mV4l2Driver->getControl(&control)) {
		VIDC_ERR("V4l2Decoder::getFrameRate, get control failed\n");
		return 0;
	}
	mFrameRate = control.value / (float)65536;
	return mFrameRate;
}

int V4l2Decoder::configureInput() {
	struct v4l2_format fmt;
	struct v4l2_requestbuffers reqBufs;
	struct v4l2_control ctrl;
	bool thumbnailMode = false;

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = INPUT_MPLANE;
	if (mV4l2Driver->getFormat(&fmt)) {
		VIDC_ERR("V4l2Decoder::configureInput, get input format failed\n");
		return -EINVAL;
	}

	fmt.fmt.pix_mp.width = mWidth;
	fmt.fmt.pix_mp.height = mHeight;
	fmt.fmt.pix_mp.pixelformat = mCodec;
	if (mV4l2Driver->setFormat(&fmt))
		return -EINVAL;
	mStride = fmt.fmt.pix_mp.plane_fmt[0].bytesperline;
	mScanline = fmt.fmt.pix_mp.height;
	mInputSize = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

	memset(&reqBufs, 0, sizeof(reqBufs));
	reqBufs.type = INPUT_MPLANE;
	reqBufs.memory = V4L2_MEMORY_DMABUF;
	reqBufs.count = VIDEO_MAX_FRAME;
	if (mV4l2Driver->reqBufs(&reqBufs))
		return -EINVAL;

	/*
	 * reqbufs call updates input buffer size and buffer counts.
	 * query driver to obtain latest values.
	 */
	memset(&fmt, 0, sizeof(fmt));
	fmt.type = INPUT_MPLANE;
	if (mV4l2Driver->getFormat(&fmt))
		return -EINVAL;
	mInputSize = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

	ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_OUTPUT;
	if (mV4l2Driver->getControl(&ctrl))
		return -EINVAL;
	mMinInputCount = ctrl.value;

	/* Input buffer_count should be 1 for thumbnail case */
	ctrl.id = V4L2_CID_MPEG_VIDC_THUMBNAIL_MODE;
	if (mV4l2Driver->getControl(&ctrl))
		return -EINVAL;
	thumbnailMode = ctrl.value;
	if (thumbnailMode && mMinInputCount != 1) {
		VIDC_ERR("V4l2Decoder::configureInput, thumbnail case, input count not 1, received %d \n", mMinInputCount);
		return -EINVAL;
	}
	if (mActualInputCount < mMinInputCount) {
		VIDC_MED("V4l2Decoder::configureInput, update input count from %d to %d\n",
			mActualInputCount, mMinInputCount);
		mActualInputCount = mMinInputCount;
	}

	if (mV4l2Driver->isInputMetadataEnabled() &&
		mV4l2Driver->isInputMetaPortEnabled()) {
		reqBufs.type = INPUT_META_PLANE;
		if (mV4l2Driver->reqBufs(&reqBufs))
			return -EINVAL;

		memset(&fmt, 0, sizeof(fmt));
		fmt.type = INPUT_META_PLANE;
		if (mV4l2Driver->getFormat(&fmt))
			return -EINVAL;
		mMetaInputSize = fmt.fmt.meta.buffersize;
	} else {
		mMetaInputSize = ALIGN(16 * 1024, SZ_4K);
	}

	return 0;
}

int V4l2Decoder::configureOutput() {
	bool found = false;
	bool thumbnailMode = false;
	bool hasResolutionChanged = false;
	struct v4l2_frmsizeenum fsize;
	struct v4l2_fmtdesc fmtdesc;
	struct v4l2_requestbuffers reqBufs;
	struct v4l2_control ctrl;
	struct v4l2_queryctrl queryctrl;
	struct v4l2_selection sel;
	int rc = 0;
	uint32_t numNotification = 0;
	uint32_t lineCount = 0;

	memset(&mOutputFormat, 0, sizeof(mOutputFormat));
	mOutputFormat.type = OUTPUT_MPLANE;
	if (mV4l2Driver->getFormat(&mOutputFormat))
		return -EINVAL;
	mOutputColorPrimaries = mOutputFormat.fmt.pix_mp.colorspace;
	mOutputMatrixCoeff = mOutputFormat.fmt.pix_mp.ycbcr_enc;
	mOutputTransferChar = mOutputFormat.fmt.pix_mp.xfer_func;
	mOutputVideoRange = mOutputFormat.fmt.pix_mp.quantization;


	auto isEarlyNotifyEnabled = [this] () -> bool {
		struct v4l2_control ctrl;
		struct v4l2_queryctrl queryctrl;
		int ret;

		queryctrl.id = V4L2_CID_MPEG_VIDC_EARLY_NOTIFY_ENABLE;
		ret = mV4l2Driver->queryControl(&queryctrl);
		if (ret) {
			VIDC_MED("V4l2Decoder::configureOutput, queryControl failed\n");
			return false;
		}

		ctrl.id = V4L2_CID_MPEG_VIDC_EARLY_NOTIFY_ENABLE;
		ret = mV4l2Driver->getControl(&ctrl);
		if (ret) {
			VIDC_MED("V4l2Decoder::configureOutput, getControl failed\n");
			return false;
		}

		return ctrl.value == 1 ? true : false;
	};

	/*
	 * Configure Line count (V4L2_CID_MPEG_VIDC_EARLY_NOTIFY_LINE_COUNT)
	 * when resolution changes during IPSC
	 */
	if (isEarlyNotifyEnabled()) {
		if (detectResolutionChange(&hasResolutionChanged))
			return -EINVAL;

		numNotification = mV4l2Driver->getEarlyNotifyIntrptCount();
		queryctrl.id = V4L2_CID_MPEG_VIDC_EARLY_NOTIFY_LINE_COUNT;

		rc = mV4l2Driver->queryControl(&queryctrl);
		if (rc && queryctrl.step != 0)
			return -EINVAL;

		lineCount = ALIGN(mHeight / numNotification, queryctrl.step);
		ctrl.id = V4L2_CID_MPEG_VIDC_EARLY_NOTIFY_LINE_COUNT;
		ctrl.value = lineCount;
		rc = mV4l2Driver->setControl(&ctrl);
		if (rc) {
			VIDC_ERR("V4l2Decoder::configureOutput, setControl failed\n");
			return -EINVAL;
		}
	}

	/* check if driver supports client requested colorfomat */
	memset(&fmtdesc, 0, sizeof(fmtdesc));
	fmtdesc.index = 0;
	fmtdesc.type = OUTPUT_MPLANE;
	while (!mV4l2Driver->enumFormat(&fmtdesc)) {
		if (fmtdesc.pixelformat == mColorFormat) {
			found = true;
			break;
		}
		fmtdesc.index++;
	}
	if (!found) {
		VIDC_ERR("V4l2Decoder::configureOutput, client colorformat %#x not supported\n", mColorFormat);
		return -ENOTSUP;
	}

	mOutputFormat.fmt.pix_mp.pixelformat = mColorFormat;
	if (mV4l2Driver->setFormat(&mOutputFormat))
		return -EINVAL;
	mStride = mOutputFormat.fmt.pix_mp.plane_fmt[0].bytesperline;
	mScanline = mOutputFormat.fmt.pix_mp.height;
	mOutputSize = mOutputFormat.fmt.pix_mp.plane_fmt[0].sizeimage;

	/* query driver recommended framesizes */
	memset(&fsize, 0, sizeof(fsize));
	fsize.index = 0;
	fsize.pixel_format = mColorFormat;
	rc = mV4l2Driver->enumFramesize(&fsize);
	if (rc)
		return -ENOTSUP;

	memset(&sel, 0, sizeof(sel));
	sel.type = OUTPUT_MPLANE;
	sel.target = V4L2_SEL_TGT_COMPOSE;
	if (mV4l2Driver->getSelection(&sel))
		return -EINVAL;
	mCropLeft = sel.r.left;
	mCropTop = sel.r.top;
	mCropWidth = sel.r.width;
	mCropHeight = sel.r.height;

	memset(&reqBufs, 0, sizeof(reqBufs));
	reqBufs.type = OUTPUT_MPLANE;
	reqBufs.memory = V4L2_MEMORY_DMABUF;
	reqBufs.count = VIDEO_MAX_FRAME;
	if (mV4l2Driver->reqBufs(&reqBufs))
		return -EINVAL;

	/*
	 * reqbufs call updates output buffer size and buffer counts.
	 * query driver to obtain latest values.
	 */
	memset(&mOutputFormat, 0, sizeof(mOutputFormat));
	mOutputFormat.type = OUTPUT_MPLANE;
	if (mV4l2Driver->getFormat(&mOutputFormat))
		return -EINVAL;
	mOutputSize = mOutputFormat.fmt.pix_mp.plane_fmt[0].sizeimage;

	ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
	if (mV4l2Driver->getControl(&ctrl))
		return -EINVAL;
	mMinOutputCount = ctrl.value;

	/* Output buffer_count should be 1 for thumbnail case */
	ctrl.id = V4L2_CID_MPEG_VIDC_THUMBNAIL_MODE;
	if (mV4l2Driver->getControl(&ctrl))
		return -EINVAL;
	thumbnailMode = ctrl.value;
	if (thumbnailMode && mCodec != V4L2_PIX_FMT_VP9 && mMinOutputCount != 1) {
		VIDC_ERR("V4l2Decoder::configureOutput, thumbnail case, output count not 1, received %d \n", mMinOutputCount);
		return -EINVAL;
	}
	if (mActualOutputCount < mMinOutputCount) {
		VIDC_MED("V4l2Decoder::configureOutput, update output count from %d to %d\n",
			mActualOutputCount, mMinOutputCount);
		mActualOutputCount = mMinOutputCount;
	}

	if (mV4l2Driver->isOutputMetadataEnabled() &&
		mV4l2Driver->isOutputMetaPortEnabled()) {
		reqBufs.type = OUTPUT_META_PLANE;
		if (mV4l2Driver->reqBufs(&reqBufs))
			return -EINVAL;
		memset(&mOutputFormat, 0, sizeof(mOutputFormat));
		mOutputFormat.type = OUTPUT_META_PLANE;
		if (mV4l2Driver->getFormat(&mOutputFormat))
			return -EINVAL;
		mMetaOutputSize = mOutputFormat.fmt.meta.buffersize;
	} else {
		mMetaOutputSize = ALIGN(16 * 1024, SZ_4K);
	}

	return 0;
}

struct v4l2_format* V4l2Decoder::getOutputFormat() {
	return &mOutputFormat;
}

static inline bool isLinearColorFmt(unsigned int colorformat) {
	return colorformat == V4L2_PIX_FMT_NV12 ||
		colorformat == V4L2_PIX_FMT_NV21 ||
		colorformat == V4L2_PIX_FMT_P010 ||
		colorformat == V4L2_PIX_FMT_RGBA32;
}

static inline bool isCompressedColorFmt(unsigned int colorformat) {
	return colorformat == V4L2_PIX_FMT_QC08C ||
		colorformat == V4L2_PIX_FMT_QC10C;
}

int V4l2Decoder::detectBitDepthChange() {
	bool isCompressedFmt, found = false;
	struct v4l2_fmtdesc fmtdesc;
	int driverSupportedFmts[MAX_COLOR_FMTS] = {0};

	/* check if driver supports client requested colorfomat */
	memset(&fmtdesc, 0, sizeof(fmtdesc));
	fmtdesc.index = 0;
	fmtdesc.type = OUTPUT_MPLANE;
	while (!mV4l2Driver->enumFormat(&fmtdesc)) {
		driverSupportedFmts[fmtdesc.index] = fmtdesc.pixelformat;
		if (fmtdesc.pixelformat == mColorFormat)
			return false;

		fmtdesc.index++;
	}

	/* check bitdepth change */
	isCompressedFmt = isCompressedColorFmt(mColorFormat);
	for (int i = 0; i < fmtdesc.index; i++) {
		if ((isCompressedFmt && isCompressedColorFmt(driverSupportedFmts[i])) ||
			(!isCompressedFmt && isLinearColorFmt(driverSupportedFmts[i]))) {
				found = true;
				mColorFormat = driverSupportedFmts[i];
				break;
			}
	}
	if (!found)
		VIDC_ERR("V4l2Decoder::detectBitDepthChange, client colorformat %#x not supported\n", mColorFormat);

	return true;
}

int V4l2Decoder::detectFilmGrainChange(bool *hasFilmGrainChanged) {
	if (mCodec != V4L2_PIX_FMT_AV1)
		return 0;

	struct v4l2_control ctrl;
	memset(&ctrl, 0, sizeof(ctrl));

	ctrl.id = V4L2_CID_MPEG_VIDC_FILM_GRAIN_PRESENT;
	if (mV4l2Driver->getControl(&ctrl))
		return -EINVAL;

	if (ctrl.value != mFgPresent) {
		VIDC_MED("V4l2Decoder::detectFilmGrainChange, Update film grain from %d to %d\n", mFgPresent, ctrl.value);
		mFgPresent = ctrl.value;
		*hasFilmGrainChanged = true;
	}

	return 0;
}

int V4l2Decoder::detectResolutionChange(bool *hasResolutionChanged) {
	struct v4l2_format fmt;
	int width, height;

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = INPUT_MPLANE;
	if (mV4l2Driver->getFormat(&fmt))
		return -EINVAL;

	width = fmt.fmt.pix_mp.width;
	height = fmt.fmt.pix_mp.height;
	if (mWidth != width || mHeight != height) {
		VIDC_MED("V4l2Decoder::detectResolutionChange, Update bitstream resolution to wxh %dx%d from %dx%d\n",
			width, height, mWidth, mHeight);
		mWidth = width;
		mHeight = height;
		*hasResolutionChanged = true;
	}

	return 0;
}

int V4l2Decoder::reconfigureOutput() {
	int rc = 0;
	int latestOutputSize;
	int latestOutputMinCount;
	struct v4l2_format fmt;
	struct v4l2_control ctrl;
	bool thumbnailMode = false;
	bool isBitDepthChanged = false;
	bool hasFilmGrainChanged = false;
	bool hasResolutionChanged = false;

	memset(&fmt, 0, sizeof(fmt));
	fmt.type = OUTPUT_MPLANE;
	if (mV4l2Driver->getFormat(&fmt))
		return -EINVAL;
	latestOutputSize = fmt.fmt.pix_mp.plane_fmt[0].sizeimage;

	memset(&ctrl, 0, sizeof(ctrl));
	ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
	if (mV4l2Driver->getControl(&ctrl))
		return -EINVAL;
	latestOutputMinCount = ctrl.value;

	/* Output buffer_count should be 1 for thumbnail case */
	ctrl.id = V4L2_CID_MPEG_VIDC_THUMBNAIL_MODE;
	if (mV4l2Driver->getControl(&ctrl))
		return -EINVAL;
	thumbnailMode = ctrl.value;
	if (thumbnailMode && mCodec != V4L2_PIX_FMT_VP9 && latestOutputMinCount != 1) {
		VIDC_ERR("V4l2Decoder::reconfigureOutput, thumbnail case, output count not 1, received %d \n", latestOutputMinCount);
		return -EINVAL;
	}

	isBitDepthChanged = detectBitDepthChange();

	/* When film grain change is detected, full reconfigure must be performed */
	if (detectFilmGrainChange(&hasFilmGrainChanged))
		return -EINVAL;

	/* When bitstream resolution change is detected, full reconfigure must be performed */
	if (detectResolutionChange(&hasResolutionChanged))
		return -EINVAL;

	VIDC_MED("V4l2Decoder::reconfigureOutput, curent min cnt %d, latest min cnt %d\n",
		mMinOutputCount, latestOutputMinCount);
	VIDC_MED("V4l2Decoder::reconfigureOutput, current o/p buffersize %d, latest output size %d, \n",
		mOutputSize, latestOutputSize);
	if (latestOutputMinCount <= mMinOutputCount &&
			latestOutputSize <= mOutputSize &&
			!isBitDepthChanged && !hasFilmGrainChanged &&
			!hasResolutionChanged) {
		std::shared_ptr<v4l2_buffer> buf;
		std::shared_ptr<v4l2_buffer> metaBuf;
		int i = mMinOutputCount;

		for (i = mMinOutputCount; i < latestOutputMinCount; i++) {
			buf = allocateBuffer(i, OUTPUT_PORT, mOutputSize);
			metaBuf = allocateMetaBuffer(i, OUTPUT_META_PORT, mMetaOutputSize);
			{
				std::unique_lock<std::mutex> lock(mBufLock);
				mOutputBufs.push_back(buf);
				if (metaBuf)
					mMetaOutputBufs.push_back(metaBuf);
			}
		}
		rc = resume();
		if (rc)
			return rc;
	}
	else {

		rc = stopOutput();
		if (rc)
			return rc;

		rc = stopMetaOutput();
		if (rc)
			return rc;

		rc = configureOutput();
		if (rc)
			return rc;

		freeMetaBuffers(OUTPUT_META_PORT);
		freeBuffers(OUTPUT_PORT);

		rc = allocateMetaBuffers(OUTPUT_META_PORT);
		if (rc)
			return rc;

		rc = allocateBuffers(OUTPUT_PORT);
		if (rc)
			return rc;

		rc = startMetaOutput();
		if (rc)
			return rc;

		rc = startOutput();
		if (rc)
			return rc;
	}
	return 0;
}

int V4l2Decoder::drain() {
	struct v4l2_decoder_cmd decCmd;
	memset(&decCmd, 0, sizeof(decCmd));
	decCmd.cmd = V4L2_DEC_CMD_STOP;
	return mV4l2Driver->decCommand(&decCmd);
}

int V4l2Decoder::resume() {
	struct v4l2_decoder_cmd decCmd;
	memset(&decCmd, 0, sizeof(decCmd));
	decCmd.cmd = V4L2_DEC_CMD_START;
	return mV4l2Driver->decCommand(&decCmd);
}

int V4l2Decoder::getFenceFd(int fence_id) {
	struct v4l2_control control;
	int ret = 0;

	memset(&control, 0, sizeof(control));
	control.id = V4L2_CID_MPEG_VIDC_OUTPUT_TX_FENCE_ID;
	control.value = fence_id;
	ret = mV4l2Driver->setControl(&control);
	if (ret)
		return ret;

	memset(&control, 0, sizeof(control));
	control.id = V4L2_CID_MPEG_VIDC_OUTPUT_TX_FENCE_FD;
	ret = mV4l2Driver->getControl(&control);
	if (ret)
		return ret;

	return control.value;
}

int V4l2Decoder::getFenceFds(struct V4L2OutputFenceInfo* fenceInfo) {
	int ret = 0, fd = -1;

	if (fenceInfo == nullptr) {
		VIDC_ERR("V4l2Decoder::getFenceFds, invalid parm\n");
		return -EINVAL;
	}

	for (int id : fenceInfo->fenceIds) {
		fd = getFenceFd(id);
		if (fd < 0) {
			VIDC_ERR("V4l2Decoder::getFenceFds, invalid fence fd %d\n", fd);
			return -EINVAL;
		}
		fenceInfo->fenceFds.push_back(fd);
	}

	return ret;
}

int V4l2Decoder::registerCallbacks(std::shared_ptr<V4l2CodecCb> cb) {
	VIDC_MED("V4l2Codec::registerCallbacks\n");
	mCb = cb;

	std::shared_ptr<V4l2Callback> v4l2cb = std::make_shared<V4l2Callback>(this);
	return mV4l2Driver->registerCallbacks(v4l2cb);
}

