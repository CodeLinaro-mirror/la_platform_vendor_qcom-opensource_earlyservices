/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/media.h>

#ifdef _LINUX_VENV_
#include <string>
#include <BufferAllocator/BufferAllocator.h>
#endif

#ifdef ANDROID
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <poll.h>
#ifndef LAHAINA_VIDEO_KGTEST
#include <BufferAllocator/BufferAllocator.h>
#endif
#elif _LINUX_VENV_
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/poll.h>
#else
#include <linux/poll.h>
#endif

#include <iostream>
#include <dirent.h>
#include <sys/stat.h>

#include "../inc/V4l2Driver.h"
#include "../inc/VidcLog.h"
#include "../inc/Utils.h"

using namespace early_video_app;

#ifdef _LINUX_VENV_
#define poll(x,y,z) vidc_poll(x,y,z)
#endif

//define wait timeout 200 milliseconds.
#define MAX_WAIT_TIMEOUT 200000


const char* v4l2_type_name(int id) {
	const char* name = "unknown";

	switch (id) {
		case OUTPUT_MPLANE:
			name = "output";
			break;
		case INPUT_MPLANE:
			name = "input";
			break;
		case OUTPUT_META_PLANE:
			name = "meta output";
			break;
		case INPUT_META_PLANE:
			name = "meta input";
			break;
		default:
			break;
	}

	return name;
}

const char* ctrl_name(int id) {
	const char* name = "unknown";

	switch (id) {
		case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
			name = "min output";
			break;
		case V4L2_CID_MIN_BUFFERS_FOR_OUTPUT:
			name = "min input";
			break;
		case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
			name = "HEVC Profile";
			break;
		case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
			name = "HEVC Level";
			break;
		case V4L2_CID_MPEG_VIDEO_HEVC_TIER:
			name = "HEVC Tier";
			break;
		case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
			name = "Header Mode";
			break;
		case V4L2_CID_MPEG_VIDEO_BITRATE:
			name = "Bitrate";
			break;
		case V4L2_CID_MPEG_VIDEO_BITRATE_MODE:
			name = "Bitrate Mode";
			break;
		case V4L2_CID_MPEG_VIDC_CODEC_CONFIG:
			name = "Codec Config";
			break;
		case V4L2_CID_MPEG_VIDC_MIN_BITSTREAM_SIZE_OVERWRITE:
			name = "Bitstream Size Overwrite";
			break;
		case V4L2_CID_MPEG_VIDC_THUMBNAIL_MODE:
			name = "Thumbnail Mode";
			break;
		case V4L2_CID_MPEG_VIDC_FRAME_RATE:
			name = "Frame Rate";
			break;
		case V4L2_CID_MPEG_VIDC_OPERATING_RATE:
			name = "Operating Rate";
			break;
		case V4L2_CID_MPEG_VIDC_OUTPUT_TX_FENCE_ID:
			name = "Fence ID";
			break;
		case V4L2_CID_MPEG_VIDC_OUTPUT_TX_FENCE_FD:
			name = "Fence FD";
			break;
		case V4L2_CID_MPEG_VIDC_LAST_FLAG_EVENT_ENABLE:
			name = "Last Flag Event";
			break;
		case V4L2_CID_MPEG_VIDC_SIGNAL_COLOR_INFO:
			name = "Signal Color Info";
			break;
		default:
			break;
	}

	return name;
}

void print_v4l2_buffer(const char* string, struct v4l2_buffer* b) {
	//comment out following print due to SEGV_MAPERR error during early lunching
	// if (b->type == OUTPUT_META_PLANE ||
	// 	b->type == INPUT_META_PLANE) {
	// 	VIDC_MED("%s: %s: idx %2d fd %u size %8d filled %8d flags %#8x\n",
	// 		string, v4l2_type_name(b->type), b->index, b->m.fd,
	// 		b->length, b->bytesused, b->flags);
	// }
	// else if (b->type == OUTPUT_MPLANE ||
	// 		b->type == INPUT_MPLANE) {
	// 	VIDC_MED("%s: %s: idx %2d fd %u req_fd %d off %8d size %8d filled %8d flags %#8x\n",
	// 		string, v4l2_type_name(b->type), b->index, b->m.planes[0].m.fd,
	// 		b->request_fd, b->m.planes[0].data_offset, b->m.planes[0].length,
	// 		b->m.planes[0].bytesused, b->flags);
	// }
}

void V4l2Driver::setEarlyNotifyIntrptCount(uint32_t count) {
	mEarlyNotifyIntrptCount = count;
}

uint32_t V4l2Driver::getEarlyNotifyIntrptCount() {
	return mEarlyNotifyIntrptCount;
}

void V4l2Driver::enableInputRequest(bool enable) {
	mInputRequestEnabled = enable;
}

bool V4l2Driver::isInputRequestEnabled() {
	return mInputRequestEnabled;
}

void V4l2Driver::enableInputMetaPort(bool enable) {
	mInputMetaPortEnabled = enable;
}

bool V4l2Driver::isInputMetaPortEnabled() {
	return mInputMetaPortEnabled;
}

void V4l2Driver::enableOutputMetaPort(bool enable) {
	mOutputMetaPortEnabled = enable;
}

bool V4l2Driver::isOutputMetaPortEnabled() {
	return mOutputMetaPortEnabled;
}

void V4l2Driver::enableInputMetadata(bool enable) {
	mInputMetadataEnabled = enable;
}

bool V4l2Driver::isInputMetadataEnabled() {
	return mInputMetadataEnabled;
}

void V4l2Driver::enableOutputMetadata(bool enable) {
	mOutputMetadataEnabled = enable;
}

bool V4l2Driver::isOutputMetadataEnabled() {
	return mOutputMetadataEnabled;
}

void V4l2Driver::enableOutBufFence(bool enable) {
	mOutBufFenceEnabled = enable;
}

bool V4l2Driver::isOutBufFenceEnabled() {
	return mOutBufFenceEnabled;
}

int V4l2Driver::ionAlloc(int size)
{
	int fd = -1;

#if !defined ANDROID && !defined _LINUX_VENV_
    // TODO(PC): implement BufferAllocator for off-target instead
	int rc = 0;
	struct ion_allocation_data alloc_data;

	if (mIonFd < 0) {
#ifdef ANDROID
		mIonFd = open("/dev/ion", 0);
		if (mIonFd < 0) {
			VIDC_ERR("V4l2Driver: failed to open ion device with error %d\n", mIonFd);
			return -EINVAL;
		}
#endif
	}
	alloc_data.flags = ION_FLAG_CACHED;
	alloc_data.heap_id_mask = ION_HEAP(ION_SYSTEM_HEAP_ID);
	alloc_data.align = 4096;
	alloc_data.len = size;
	rc = ion_alloc_fd(mIonFd, alloc_data.len, alloc_data.align,
		alloc_data.heap_id_mask, alloc_data.flags, &fd);
	if (rc < 0) {
		VIDC_ERR("V4l2Driver: ION ALLOC failed\n");
		return -EINVAL;
	}
#else // ANDROID
	BufferAllocator bufferAllocator;
	fd = bufferAllocator.Alloc("qcom,system", size, 0);
#endif // ANDROID

	return fd;
}

void V4l2Driver::ionFree(int fd) {
#if defined ANDROID || defined _LINUX_VENV_
	close(fd);
#else
	ion_close(fd);
#endif
}

int IOCTL(int fd, unsigned int cmd, void* arg) {
	int rc = 0;

#ifdef ANDROID
	rc = ioctl(fd, cmd, arg);
#elif _LINUX_VENV_
	rc = ioctl(fd, cmd, arg);
#else
	rc = vidc_ioctl(fd, cmd, arg);
#endif

	return rc;
}

int V4l2Driver::Open(unsigned int type) {
	VIDC_HIGH("V4l2Driver::Open, enter\n");

	std::string devPath;
	if (type == V4L2_CODEC_TYPE_DECODER) {
		devPath = "/dev/video32";
	}
	else if (type == V4L2_CODEC_TYPE_ENCODER) {
		devPath = "/dev/video33";
	}
	else {
		return -EINVAL;
	}
	scanDevDirectory("/dev");
	scanDevDirectory("/dev/dri");

	printKPILog("%s%s%s", LogKPITag, LogAPPTag, "open video driver device");
#ifdef ANDROID
	VIDC_HIGH("V4l2Driver::Open, android env\n");
	mFd = open(devPath.c_str(), O_RDWR);
#elif _LINUX_VENV_
	VIDC_HIGH("V4l2Driver::Open, linux env\n");
	mFd = open(devPath.c_str(), O_RDWR);
#else
	VIDC_HIGH("V4l2Driver::Open, not linux env\n");
	mFd = vidc_open(devPath.c_str(), O_RDWR);
#endif
	if (mFd < 0) {
		printKPILog("%s%s%s", LogKPITag, LogAPPTag, "open video driver device failed");
		VIDC_ERR("V4l2Driver::Open, Failed to open %s\n", devPath.c_str());
		return -EINVAL;
	}
	printKPILog("%s%s%s", LogKPITag, LogAPPTag, "open video driver device done, app ready");
	VIDC_HIGH("V4l2Driver::Open, open %s, driver fd %d\n",  devPath.c_str(), mFd);

	return 0;
}

int V4l2Driver::openMediaDevice(unsigned int type) {
	VIDC_MED("V4l2Driver::openMediaDevice, type = %d\n", type);

	int rc = 0;
	int numTries = 25;
	int numMediaDevices = 0;
	char mediaDevName[256];
	struct media_device_info mediaDevInfo;

	/*
	 * For encoder, create media fd is used to send dynamic configs
	 * for frame synchronization and queueBufferRequest() function is
	 * called. But this function is very costly with respect to MIPS
	 * since this will invoke IOCTL which will internally call multiple
	 * sys calls followed by v4l2 calls including __kmalloc as well hence
	 * increasing the instruction counts.
	 * All the configs can be sent directly through metadata buffers since
	 * each queueBuffer will have corresponding metaData buffer. And other
	 * configs which are required by driver can be set through s_ctrl as well.
	 * Fallback to queueBuf() function instead of queueBufferRequest()
	 * is less costly, hence media fd creation is disabled for encoder.
	 */
	if (type == V4L2_CODEC_TYPE_DECODER ||type == V4L2_CODEC_TYPE_ENCODER) {
		return rc;
	}

#ifdef _LINUX_VENV_
    return 0;
#endif

	while (numTries) {
		numTries--;
		snprintf(mediaDevName, sizeof(mediaDevName), "/dev/media%d", numMediaDevices++);
		mMediaFd = open(mediaDevName, O_RDWR);
		if (mMediaFd < 0) {
			VIDC_ERR("V4l2Driver::openMediaDevice, failed to open media device with error %d\n", mMediaFd);
			rc = -EINVAL;
			break;
		}
		rc = IOCTL(mMediaFd, MEDIA_IOC_DEVICE_INFO, &mediaDevInfo);
		if (rc < 0) {
			VIDC_ERR("V4l2Driver::openMediaDevice, MEDIA_IOC_DEVICE_INFO failed with error %d\n", rc);
			close(mMediaFd);
			mMediaFd = -1;
			rc = -EINVAL;
			break;
		}
		if (strncmp(mediaDevInfo.model, "msm_vidc_media", sizeof(mediaDevInfo.model)) != 0) {
			close(mMediaFd);
			mMediaFd = -1;
		}
		else {
			VIDC_MED("V4l2Driver::openMediaDevice, open media device: \"%s\", fd %d\n", mediaDevInfo.model, mMediaFd);
			enableInputRequest(true);
			break;
		}
	}

	return rc;
}

void V4l2Driver::Close() {
	if (mFd < 0) {
		return;
	}
	VIDC_MED("V4l2Driver::Close, close driver fd %d\n", mFd);
#ifdef ANDROID
	close(mFd);
#elif _LINUX_VENV_
	close(mFd);
#else
	vidc_close(mFd);
#endif
	mFd = -1;
}

void V4l2Driver::closeMediaDevice() {
	if (mMediaFd < 0) {
		return;
	}
	VIDC_MED("V4l2Driver::closeMediaDevice, close media driver fd %d\n", mMediaFd);
#ifdef ANDROID
	close(mMediaFd);
#endif
	mMediaFd = -1;
}

int V4l2Driver::queryControl(struct v4l2_queryctrl* queryctrl) {
	VIDC_MED("V4l2Driver::queryControl\n");
	int rc = IOCTL(mFd, VIDIOC_QUERYCTRL, queryctrl);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::queryControl, queryCotrol failed for \"%s\"\n", ctrl_name(queryctrl->id));
		rc = -EINVAL;
	}
	else {
		VIDC_MED("V4l2Driver::queryControl, name: \"%s\", min: %d, max: %d, step: %d, default: %d\n",
			queryctrl->name, queryctrl->minimum, queryctrl->maximum,
			queryctrl->step, queryctrl->default_value);
	}
	return rc;
}

int V4l2Driver::queryMenu(struct v4l2_querymenu* querymenu) {
	VIDC_MED("V4l2Driver::queryMenu\n");
	int rc = IOCTL(mFd, VIDIOC_QUERYMENU, querymenu);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::queryMenu, failed for \"%s\"\n", ctrl_name(querymenu->id));
		rc = -EINVAL;
	}
	else {
		VIDC_MED("V4l2Driver::queryMenu, name: \"%s\", id: %#x, index: %d\n",
			querymenu->name, querymenu->id, querymenu->index);
	}
	return rc;
}

int V4l2Driver::queryCapabilities(struct v4l2_capability *caps) {
	VIDC_MED("V4l2Driver::queryCapabilities\n");
	int rc = IOCTL(mFd, VIDIOC_QUERYCAP, caps);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::queryCapabilities, Failed to query capabilities\n");
		rc = -EINVAL;
	}
	else {
		//comment out following print due to SEGV_MAPERR error during early lunching
		// VIDC_MED("V4l2Driver::queryCapabilities, driver name: %s, card: %s, bus_info: %s, "
		// 	"version: %d, capabilities: %#x, device_caps: %#x\n",
		// 	caps->driver, caps->card, caps->bus_info,
		// 	caps->version, caps->capabilities, caps->device_caps);
	}
	return rc;
}

int V4l2Driver::enumFormat(struct v4l2_fmtdesc* fmtDesc) {
	VIDC_MED("V4l2Driver::enumFormat\n");
	int rc = IOCTL(mFd, VIDIOC_ENUM_FMT, fmtDesc);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::enumFormat, ended for index %d\n", fmtDesc->index);
		rc = -ENOTSUP;
	}
	else {
		//comment out following print due to SEGV_MAPERR error during early lunching
		// VIDC_MED("V4l2Driver::enumFormat, index %d, description: \"%s\", pixelFmt: %#x, flags: %#x\n",
		// 	fmtDesc->index, fmtDesc->description, fmtDesc->pixelformat, fmtDesc->flags);
	}
	return rc;
}

int V4l2Driver::enumFramesize(struct v4l2_frmsizeenum* frameSize) {
	VIDC_MED("V4l2Driver::enumFramesize\n");
	int rc = IOCTL(mFd, VIDIOC_ENUM_FRAMESIZES, frameSize);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::enumFramesize, failed for pixel_format %#x\n", frameSize->pixel_format);
		rc = -EINVAL;
		return rc;
	}
	if (frameSize->type != V4L2_FRMSIZE_TYPE_STEPWISE) {
		VIDC_ERR("V4l2Driver::enumFramesize, type (%d) returned in not stepwise\n", frameSize->type);
		rc = -EINVAL;
	}
	else {
		VIDC_MED("V4l2Driver::enumFramesize, [%u x %u] to [%u x %u]\n",
			frameSize->stepwise.min_width, frameSize->stepwise.min_height,
			frameSize->stepwise.max_width, frameSize->stepwise.max_height);
	}

	return rc;
}

int V4l2Driver::enumFrameInterval(struct v4l2_frmivalenum *fival) {
	VIDC_MED("V4l2Driver::enumFrameInterval\n");
	int rc = IOCTL(mFd, VIDIOC_ENUM_FRAMEINTERVALS, fival);
	if (rc) {
		VIDC_ERR("V4l2Driver::enumFrameInterval, failed for pixel_format %#x, resoltion [%u x %u]\n",
			fival->pixel_format, fival->width, fival->height);
		rc = -EINVAL;
		return rc;
	}
	if (fival->type != V4L2_FRMIVAL_TYPE_STEPWISE) {
		VIDC_ERR("V4l2Driver::enumFrameInterval, type (%d) returned in not stepwise\n", fival->type);
		rc = -EINVAL;
	}
	else {
		VIDC_MED("V4l2Driver::enumFrameInterval, resoltion [%u x %u], interval [%u / %u] to [%u / %u]\n",
			fival->width, fival->height,
			fival->stepwise.min.numerator, fival->stepwise.min.denominator,
			fival->stepwise.max.numerator, fival->stepwise.max.denominator);
	}

	return rc;
}

int V4l2Driver::getFormat(struct v4l2_format* fmt) {
	VIDC_MED("V4l2Driver::getFormat\n");
	int rc = IOCTL(mFd, VIDIOC_G_FMT, fmt);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::getFormat, failed for type %d\n", fmt->type);
		rc = -EINVAL;
	}
	else{
		VIDC_MED("V4l2Driver::getFormat, type %d, [wxh] %dx%d, fmt %#x, size %d\n",
			fmt->type, fmt->fmt.pix_mp.width, fmt->fmt.pix_mp.height,
			fmt->fmt.pix_mp.pixelformat, fmt->fmt.pix_mp.plane_fmt[0].sizeimage);
	}
	return rc;
}

int V4l2Driver::setFormat(struct v4l2_format* fmt) {
	VIDC_MED("V4l2Driver::setFormat\n");
	int rc = IOCTL(mFd, VIDIOC_S_FMT, fmt);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::setFormat, failed for type %d\n", fmt->type);
		rc = -EINVAL;
	}
	else {
		VIDC_MED("V4l2Driver::setFormat, type %d, [wxh] %dx%d, fmt %#x, size %d\n",
			fmt->type, fmt->fmt.pix_mp.width, fmt->fmt.pix_mp.height,
			fmt->fmt.pix_mp.pixelformat, fmt->fmt.pix_mp.plane_fmt[0].sizeimage);
	}
	return rc;
}

int V4l2Driver::decCommand(struct v4l2_decoder_cmd* cmd) {
	VIDC_MED("V4l2Driver::decCommand\n");
	int rc = IOCTL(mFd, VIDIOC_DECODER_CMD, cmd);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::decCommand, error %d\n", rc);
	}
	return rc;
}

int V4l2Driver::encCommand(struct v4l2_encoder_cmd* cmd) {
	VIDC_MED("V4l2Driver::encCommand\n");
	int rc = IOCTL(mFd, VIDIOC_ENCODER_CMD, cmd);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::encCommand, error %d\n", rc);
	}
	return rc;
}

int V4l2Driver::subscribeEvent(unsigned int event_type) {
	VIDC_MED("V4l2Driver::subscribeEvent\n");
	struct v4l2_event_subscription event;
	memset(&event, 0, sizeof(event));
	event.type = event_type;
	VIDC_MED("V4l2Driver::subscribeEvent, type %d \n", event_type);
	int rc = IOCTL(mFd, VIDIOC_SUBSCRIBE_EVENT, &event);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::subscribeEvent, error %d\n", rc);
	}
	return rc;
}

int V4l2Driver::unsubscribeEvent(unsigned int event_type) {
	VIDC_MED("V4l2Driver::unsubscribeEvent\n");
	struct v4l2_event_subscription event;
	memset(&event, 0, sizeof(event));
	event.type = event_type;
	VIDC_MED("V4l2Driver::unsubscribeEvent, type %d\n", event_type);
	int rc = IOCTL(mFd, VIDIOC_UNSUBSCRIBE_EVENT, &event);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::unsubscribeEvent, error %d\n", rc);
	}
	return rc;
}

int V4l2Driver::setParm(struct v4l2_streamparm* sparm) {
	VIDC_MED("V4l2Driver::setParm\n");
	int rc = IOCTL(mFd, VIDIOC_S_PARM, sparm);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::setParm, failed for type: %u\n", sparm->type);
		rc = -EINVAL;
	}
	else {
		if (sparm->type == INPUT_MPLANE) {
			VIDC_MED("V4l2Driver::setParm:INPUT_MPLANE, type: %u, numer: %u denom: %u\n",
			sparm->type,
			sparm->parm.output.timeperframe.numerator,
			sparm->parm.output.timeperframe.denominator);
		}
		else {
			VIDC_MED("V4l2Driver::setParm, type: %u, numer: %u denom: %u\n",
			sparm->type,
			sparm->parm.capture.timeperframe.numerator,
			sparm->parm.capture.timeperframe.denominator);
		}
	}

	return rc;
}

int V4l2Driver::getParm(struct v4l2_streamparm* sparm) {
	VIDC_MED("V4l2Driver::getParm\n");
	int rc = IOCTL(mFd, VIDIOC_G_PARM, sparm);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::getParm, failed for type: %u\n", sparm->type);
		rc = -EINVAL;
	}
	else {
		if (sparm->type == INPUT_MPLANE) {
			VIDC_MED("V4l2Driver::getParm:INPUT_MPLANE, type: %u, numer: %u denom: %u\n",
			sparm->type,
			sparm->parm.output.timeperframe.numerator,
			sparm->parm.output.timeperframe.denominator);
		}
		else {
			VIDC_MED("V4l2Driver::getParm, type: %u, numer: %u denom: %u\n",
			sparm->type,
			sparm->parm.capture.timeperframe.numerator,
			sparm->parm.capture.timeperframe.denominator);
		}
	}

	return rc;
}

int V4l2Driver::getControl(struct v4l2_control* ctrl) {
	VIDC_MED("V4l2Driver::getControl\n");
	int rc = IOCTL(mFd, VIDIOC_G_CTRL, ctrl);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::getControl, failed for \"%s\"\n", ctrl_name(ctrl->id));
		rc = -EINVAL;
	}
	else {
		VIDC_MED("V4l2Driver::getControl, \"%s\", value %d\n",
			ctrl_name(ctrl->id), ctrl->value);
	}
	return rc;
}

int V4l2Driver::setControl(struct v4l2_control* ctrl) {
	VIDC_MED("V4l2Driver::setControl\n");
	int rc = IOCTL(mFd, VIDIOC_S_CTRL, ctrl);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::setControl, failed for \"%s\"\n", ctrl_name(ctrl->id));
		rc = -EINVAL;
	}
	else {
		VIDC_MED("V4l2Driver::setControl, \"%s\", value %d\n",
			ctrl_name(ctrl->id), ctrl->value);
	}
	return rc;
}

int V4l2Driver::getSelection(struct v4l2_selection* sel) {
	VIDC_MED("V4l2Driver::getSelection\n");
	int rc = IOCTL(mFd, VIDIOC_G_SELECTION, sel);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::getSelection, failed for type %d, target %d\n",
			sel->type, sel->target);
		rc = -EINVAL;
	}
	else {
		VIDC_MED("V4l2Driver::getSelection, type %d, target %d, left %d top %d width %d height %d\n",
			sel->type, sel->target, sel->r.left, sel->r.top,
			sel->r.width, sel->r.height);
	}
	return rc;
}

int V4l2Driver::setSelection(struct v4l2_selection* sel) {
	VIDC_MED("V4l2Driver::setSelection\n");
	VIDC_MED("V4l2Driver::setSelection, type %d, target %d, left %d top %d width %d height %d\n",
		sel->type, sel->target, sel->r.left, sel->r.top,
		sel->r.width, sel->r.height);
	int rc = IOCTL(mFd, VIDIOC_S_SELECTION, sel);
	if (rc != 0) {
		VIDC_MED("V4l2Driver::setSelection, failed for type %d, target %d\n",
			sel->type, sel->target);
		rc = -EINVAL;
	}
	return rc;
}

int V4l2Driver::reqBufs(struct v4l2_requestbuffers* reqbufs) {
	VIDC_MED("V4l2Driver::reqBufs\n");
	VIDC_MED("V4l2Driver::reqBufs, type %d, count %d memory %d\n",
		reqbufs->type, reqbufs->count, reqbufs->memory);
	int rc = IOCTL(mFd, VIDIOC_REQBUFS, reqbufs);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::reqBufs, failed for type %d, count %d memory %d\n",
			reqbufs->type, reqbufs->count, reqbufs->memory);
		rc = -EINVAL;
	}
	return rc;
}

int V4l2Driver::allocateRequestFd(int port) {
	VIDC_MED("V4l2Driver::allocateRequestFd\n");
	int rc = 0;
	int i;

	if (port != INPUT_MPLANE)
		return rc;

	if (mMediaFd < 0) {
		VIDC_ERR("V4l2Driver::allocateRequestFd, media device not opened yet\n");
		return -EINVAL;
	}

	std::unique_lock<std::mutex> lock(mRequestLock);
	for (i = 0; i < VIDEO_MAX_FRAME; i++) {
		rc = IOCTL(mMediaFd, MEDIA_IOC_REQUEST_ALLOC, &mRequestFd[i]);
		if (mRequestFd[i] < 0) {
			VIDC_ERR("V4l2Driver::allocateRequestFd, alloc request_fd failed with error %d\n", mRequestFd[i]);
			return -EINVAL;
		}
	}

	return rc;
}

int V4l2Driver::closeRequestFd(int port) {
	VIDC_MED("V4l2Driver::closeRequestFd\n");
	int rc = 0;
	int i;

	if (port != INPUT_MPLANE)
		return 0;

	if (mMediaFd < 0) {
		VIDC_ERR("V4l2Driver::closeRequestFd, media device not opened yet\n");
		return -EINVAL;
	}

	std::unique_lock<std::mutex> lock(mRequestLock);
	for (i = 0; i < VIDEO_MAX_FRAME; i++) {
		close(mRequestFd[i]);
		mRequestFd[i] = -1;
	}

	return rc;
}


int V4l2Driver::streamOn(int port) {
	VIDC_MED("V4l2Driver::streamOn, port %d\n", port);
	int rc = IOCTL(mFd, VIDIOC_STREAMON, &port);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::streamOn, failed for port %d\n", port);
		return -EINVAL;
	}
	return 0;
}

int V4l2Driver::streamOff(int port) {
	VIDC_MED("V4l2Driver::streamOff, port %d\n", port);
	int rc = IOCTL(mFd, VIDIOC_STREAMOFF, &port);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::streamOff, failed for port %d\n", port);
		return -EINVAL;
	}
	return 0;
}

int V4l2Driver::queueBuf(struct v4l2_buffer* b) {
	VIDC_MED("V4l2Driver::queueBuf\n");
	print_v4l2_buffer("V4l2Driver::queueBuf", b);
	int rc = IOCTL(mFd, VIDIOC_QBUF, b);
	if (rc != 0) {
		print_v4l2_buffer("V4l2Driver::queueBuf, failed to QBUF", b);
		return -EINVAL;
	}
	return 0;
}

int V4l2Driver::queueBufferRequest(struct v4l2_buffer *v4l2_buf, struct v4l2_ext_controls *controls) {
	VIDC_MED("V4l2Driver::queueBufferRequest\n");

	int rc = 0;
	int reqFd = -1;
	struct v4l2_buffer *b;
	struct v4l2_buffer buf;
	struct v4l2_plane v4l2_plane[VIDEO_MAX_PLANES];

	b = &buf;
	memcpy(b, v4l2_buf, sizeof(struct v4l2_buffer));
	b->m.planes = v4l2_plane;
	memcpy(&b->m.planes[0], &v4l2_buf->m.planes[0], sizeof(struct v4l2_plane));

	{
		std::unique_lock<std::mutex> lock(mRequestLock);
		reqFd = mRequestFd[b->index];
		if (reqFd < 0) {
			VIDC_ERR("V4l2Driver::queueBufferRequest, invalid request fd %d\n", reqFd);
			return -EINVAL;
		}

		if (controls != NULL && controls->count > 0) {
			controls->which = V4L2_CTRL_WHICH_REQUEST_VAL;
			controls->request_fd = reqFd;

			rc = IOCTL(mFd, VIDIOC_S_EXT_CTRLS, controls);
			if (rc != 0) {
				VIDC_ERR("V4l2Driver::queueBufferRequest, VIDIOC_S_EXT_CTRLS failed request_fd %d, error %d\n",
					controls->request_fd, rc);
				return rc;
			}
		}

		b->flags |= V4L2_BUF_FLAG_REQUEST_FD;
		b->request_fd = reqFd;
		print_v4l2_buffer("QBUF", b);
		rc = IOCTL(mFd, VIDIOC_QBUF, b);
		if (rc != 0) {
			print_v4l2_buffer("failed to QBUF", b);
			return -EINVAL;
		}

		rc = ioctl(reqFd, MEDIA_REQUEST_IOC_QUEUE, NULL);
		if (rc != 0) {
			print_v4l2_buffer("MEDIA_REQUEST_IOC_QUEUE failed", b);
			return -EINVAL;
		}
	}

	return 0;
}

int V4l2Driver::deQueueBuf(struct v4l2_buffer* buffer) {
	VIDC_MED("V4l2Driver::deQueueBuf\n");
	int rc = IOCTL(mFd, VIDIOC_DQBUF, buffer);
	if (rc != 0) {
		VIDC_ERR("V4l2Driver::deQueueBuf, error %d\n", rc);
	}

	return rc;
}

void V4l2Driver::dequeueInputMetaBuffersEarly() {
	VIDC_MED("V4l2Driver::dequeueInputMetaBuffersEarly\n");
	if (mCb == NULL) {
		return;
	}
	struct v4l2_buffer metabuffer;
	if (mInputMetaPortEnabled && mInputMetadataEnabled) {
		memset(&metabuffer, 0, sizeof(metabuffer));
		metabuffer.type = INPUT_META_PLANE;
		metabuffer.memory = V4L2_MEMORY_DMABUF;
		int rc = -0;
		do {
			rc = IOCTL(mFd, VIDIOC_DQBUF, &metabuffer);
			if (rc != 0) {
				break;
			}
			rc = mCb->onV4l2BufferDone(&metabuffer);
			mError = (rc != 0);
		} while (1);
	}
}

int V4l2Driver::threadLoop() {
	int rc = 0;
	struct v4l2_buffer buffer;
	struct v4l2_buffer metabuffer;
	struct v4l2_plane plane[VIDEO_MAX_PLANES];
	struct v4l2_event event;
	struct pollfd pollFds[2];

	VIDC_MED("V4l2Driver::threadLoop, begin\n");
	mThreadRunning.store(true);
	pollFds[0].events = POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM | POLLRDBAND | POLLPRI | POLLERR;
	pollFds[0].fd = mFd;

	while (!mPollThreadExit.load()) {
		memset(&event, 0, sizeof(event));
		pollFds[0].revents = 0;
		rc = poll(pollFds, 1, 2000);
		if (mCb == NULL) {
			VIDC_ERR("V4l2Driver::threadLoop, callback not set, poll event result: %d, polled event = 0x%x\n", rc, pollFds[0].revents);
			usleep(MAX_WAIT_TIMEOUT);
			continue;
		}
		if (rc == 0) {
			VIDC_MED("V4l2Driver::threadLoop, poll timedout\n");
			continue;
		}
		else if (rc < 0) {
			VIDC_ERR("V4l2Driver::threadLoop, poll error %d\n", rc);
			if (errno == EINTR || errno == EAGAIN) {
				continue;
			}
			else {
				mCb->onV4l2Error(errno);
				break;
			}
		}
		if (pollFds[0].revents & POLLERR) {
			VIDC_ERR("V4l2Driver::threadLoop, poll error received\n");
			mError = true;
			mCb->onV4l2Error(POLLERR);
			break;
		}
		if (pollFds[0].revents & POLLPRI) {
			rc = IOCTL(mFd, VIDIOC_DQEVENT, &event);
			if (rc == 0) {
				VIDC_MED("V4l2Driver::threadLoop, Received v4l2 event, type %#x\n", event.type);
				if (event.type == V4L2_EVENT_EOS) {
					VIDC_HIGH("V4l2Driver::threadLoop, Received V4L2_EVENT_EOS\n");
					onOutputPollEvent(event);
					break;
				}
				else {
					mCb->onV4l2EventDone(&event);
				}
			}
			else {
				VIDC_HIGH("V4l2Driver::threadLoop, VIDIOC_DQEVENT failed, rc = %d\n", rc);
			}
		}
		if ((pollFds[0].revents & POLLIN) || (pollFds[0].revents & POLLRDNORM)) {
			VIDC_MED("V4l2Driver::threadLoop, Received v4l2 POLLIN or POLLRDNORM\n");
			onOutputPollEvent(event);
		}
		if ((pollFds[0].revents & POLLOUT) || (pollFds[0].revents & POLLWRNORM)) {
			VIDC_MED("V4l2Driver::threadLoop, Received v4l2 POLLOUT or POLLWRNORM\n");
			memset(&buffer, 0, sizeof(buffer));
			memset(&plane[0], 0, sizeof(plane));
			buffer.type = INPUT_MPLANE;
			buffer.m.planes = plane;
			buffer.length = 1;
			buffer.memory = V4L2_MEMORY_DMABUF;
			do {
				if (mOutBufFenceEnabled) {
					dequeueInputMetaBuffersEarly();
				}
				rc = IOCTL(mFd, VIDIOC_DQBUF, &buffer);
				if (rc != 0) {
					VIDC_MED("V4l2Driver::threadLoop, Received v4l2 POLLOUT or POLLWRNORM\n");
					break;
				}
				if (mInputMetaPortEnabled && mInputMetadataEnabled) {
					memset(&metabuffer, 0, sizeof(metabuffer));
					metabuffer.type = INPUT_META_PLANE;
					metabuffer.memory = V4L2_MEMORY_DMABUF;
					rc = IOCTL(mFd, VIDIOC_DQBUF, &metabuffer);
					if (!mOutBufFenceEnabled) {
						// input meta buffers might have already been dequeued for
						// fenced session. So do not assert for fenced session
						if (rc != 0) {
							VIDC_ERR("V4l2Driver::threadLoop, fence for output buffer is not enabled\n");
							mError = true;
						}
						if (metabuffer.index != buffer.index) {
							VIDC_ERR("%V4l2Driver::threadLoop, meta buffer index not matching\n");
							mError = true;
						}
					}
					if (rc == 0) {
						rc = mCb->onV4l2BufferDone(&metabuffer);
						mError = (rc != 0);
					}
				}
				if (mInputRequestEnabled) {
					std::unique_lock<std::mutex> lock(mRequestLock);
					if (buffer.request_fd == mRequestFd[buffer.index]) {
						rc = IOCTL(buffer.request_fd, MEDIA_REQUEST_IOC_REINIT, NULL);
						if (rc != 0) {
							VIDC_ERR("V4l2Driver::threadLoop, failed to MEDIA_REQUEST_IOC_REINIT for index %d, request fd %d\n",
								buffer.index, buffer.request_fd);
							mCb->onV4l2Error(rc);
							break;
						}
					} else {
						VIDC_MED("V4l2Driver::threadLoop, request fd %d not found for index %d\n",
							buffer.request_fd, buffer.index);
					}
				}
				VIDC_MED("V4l2Driver::threadLoop, Received v4l2 POLLOUT or POLLWRNORM\n");
				rc = mCb->onV4l2BufferDone(&buffer);
				mError = (rc != 0);
			} while (!mPollOutputThreadExit.load());
		}
	}
	VIDC_MED("V4l2Driver::threadLoop, end\n");
	return 0;
}

void V4l2Driver::onOutputPollEvent(v4l2_event& event) {
	if (mPollOutputThread == nullptr) {
		createPollOutputThread();
	}
	{
		std::lock_guard<std::mutex> lock(mOutputEventVectorMutex);
		if (mPollOutputEventVector.size() > 0) {
			v4l2_event lastEvent = mPollOutputEventVector.back();
			if (event.type != lastEvent.type) {
				mPollOutputEventVector.push_back(event);
			}
		}
		else {
			mPollOutputEventVector.push_back(event);
		}
		VIDC_LOW("V4l2Driver::onOutputPollEvent, mPollOutputEventVector.size = %d\n", mPollOutputEventVector.size());
	}
	mOutputEventVectorSignal.notify_all();
}

void PollThreadFunc(V4l2Driver& driver) {
	driver.threadLoop();
}

int V4l2Driver::createPollThread() {
	VIDC_MED("V4l2Driver::createPollThread\n");
	mPollThreadExit.store(false);
	mThreadRunning.store(false);
	mPollThread = std::make_shared<std::thread>(PollThreadFunc, std::ref(*this));
	if (!mPollThread) {
		VIDC_ERR("V4l2Driver::createPollThread, poll thread create failed\n");
		return -EINVAL;
	}
	else {
		int count = 0;
		while (!mThreadRunning.load()) {
			VIDC_MED("V4l2Driver::createPollThread, wait for poll thread running\n");
			usleep(10 * 1000); // 10 ms
			count++;
			if (count >= 100)
				break;
		}
		if (!mThreadRunning.load()) {
			VIDC_ERR("V4l2Driver::createPollThread, poll thread not running\n");
			return -EINVAL;
		}
	}
	VIDC_MED("V4l2Driver::createPollThread, poll thread started\n");
	return 0;
}

int V4l2Driver::stopPollThread() {
	VIDC_MED("V4l2Driver::stopPollThread\n");
	if (!mPollThread || mPollThreadExit.load()) {
		VIDC_MED("V4l2Driver::stopPollThread, invalid poll thread. exit %d\n", mPollThreadExit.load());
		return -EINVAL;
	}

	mPollThreadExit.store(true);
	VIDC_MED("V4l2Driver::stopPollThread, join thread\n");
	if (mPollThread != nullptr and mPollThread->joinable()) {
		mPollThread->join();
	}
	mPollThread = nullptr;

	VIDC_MED("V4l2Driver::stopPollThread, exit poll thread\n");
	return 0;
}

void OutputThreadFunc(V4l2Driver& driver) {
	driver.outputThreadLoop();
}

int V4l2Driver::createPollOutputThread() {
	VIDC_MED("V4l2Driver::createPollOutputThread\n");
	if (mPollOutputThread != nullptr) {
		VIDC_MED("V4l2Driver::createPollOutputThread, poll thread already created\n");
		return -EINVAL;
	}
	mPollOutputThreadExit.store(false);
	mOutputThreadRunning.store(false);
	mPollOutputThread = std::make_shared<std::thread>(OutputThreadFunc, std::ref(*this));
	if (!mPollOutputThread) {
		VIDC_ERR("V4l2Driver::createPollOutputThread, poll thread create failed\n");
		return -EINVAL;
	}
	else {
		int count = 0;
		while (!mOutputThreadRunning.load()) {
			VIDC_MED("V4l2Driver::createPollOutputThread, wait for poll thread running\n");
			usleep(10 * 1000); // 10 ms
			count++;
			if (count >= 100)
				break;
		}
		if (!mOutputThreadRunning.load()) {
			VIDC_ERR("V4l2Driver::createPollOutputThread, poll thread not running\n");
			return -EINVAL;
		}
	}
	VIDC_MED("V4l2Driver::createPollOutputThread, poll thread started\n");
	return 0;
}

int V4l2Driver::stopPollOutputThread() {
	VIDC_MED("V4l2Driver::stopPollOutputThread\n");
	if (!mPollOutputThread || mPollOutputThreadExit.load()) {
		VIDC_MED("V4l2Driver::stopPollOutputThread, invalid poll thread. exit %d\n", mPollOutputThreadExit.load());
		return -EINVAL;
	}

	mPollOutputThreadExit.store(true);
	VIDC_MED("V4l2Driver::stopPollOutputThread, join thread\n");
	if (mPollOutputThread != nullptr and mPollOutputThread->joinable()) {
		mPollOutputThread->join();
	}
	mPollOutputThread = nullptr;

	VIDC_MED("V4l2Driver::stopPollOutputThread, exit poll thread\n");
	return 0;
}

int V4l2Driver::outputThreadLoop() {
	int rc = 0;
	struct v4l2_buffer buffer;
	struct v4l2_buffer metabuffer;
	struct v4l2_plane plane[VIDEO_MAX_PLANES];

	VIDC_MED("V4l2Driver::threadOutputLoop, begin\n");
	mOutputThreadRunning.store(true);

	while (!mPollOutputThreadExit.load()) {
		std::unique_lock<std::mutex> lock(mOutputEventVectorMutex);
		bool result = mOutputEventVectorSignal.wait_for(lock, std::chrono::microseconds(MAX_WAIT_TIMEOUT), [&]{
				return (!mPollOutputEventVector.empty() || mPollOutputThreadExit.load());
			});
		if (!result || mPollOutputThreadExit.load()) {
			lock.unlock();
			continue;
		}
		if (mCb == NULL) {
			VIDC_ERR("V4l2Driver::threadOutputLoop, callback not set\n");
			lock.unlock();
			usleep(MAX_WAIT_TIMEOUT);
			continue;
		}

		v4l2_event event = mPollOutputEventVector.front();
		mPollOutputEventVector.erase(mPollOutputEventVector.begin());
		VIDC_LOW("V4l2Driver::threadOutputLoop, mPollOutputEventVector.size = %d\n", mPollOutputEventVector.size());
		lock.unlock();
		if (event.type == V4L2_EVENT_EOS) {
			VIDC_HIGH("V4l2Driver::threadOutputLoop, V4L2_EVENT_EOS dequeue\n");
			mCb->onV4l2EventDone(&event);
			break;
		}

		VIDC_LOW("V4l2Driver::threadOutputLoop, OUTPUT_MPLANE dequeue\n");
		memset(&buffer, 0, sizeof(buffer));
		memset(&plane[0], 0, sizeof(plane));
		buffer.type = OUTPUT_MPLANE;
		buffer.m.planes = plane;
		buffer.length = 1;
		buffer.memory = V4L2_MEMORY_DMABUF;
		do {
			if (mOutBufFenceEnabled) {
				dequeueInputMetaBuffersEarly();
			}
			rc = IOCTL(mFd, VIDIOC_DQBUF, &buffer);
			if (rc != 0) {
				break;
			}
			if (mOutputMetaPortEnabled && mOutputMetadataEnabled) {
				memset(&metabuffer, 0, sizeof(metabuffer));
				metabuffer.type = OUTPUT_META_PLANE;
				metabuffer.memory = V4L2_MEMORY_DMABUF;
				rc = IOCTL(mFd, VIDIOC_DQBUF, &metabuffer);
				if (rc != 0) {
					VIDC_ERR("V4l2Driver::threadOutputLoop, VIDIOC_DQBUF failed\n");
					mError = true;
				}
				if (metabuffer.index != buffer.index) {
					VIDC_ERR("V4l2Driver::threadOutputLoop, meta buffer index not matching\n");
					mError = true;
				}
				rc = mCb->onV4l2BufferDone(&metabuffer);
				mError = (rc != 0);
			}
			rc = mCb->onV4l2BufferDone(&buffer);
			mError = (rc != 0);
		} while (!mPollOutputThreadExit.load());
	}
	VIDC_MED("V4l2Driver::threadOutputLoop, end\n");
	return 0;
}

int V4l2Driver::registerCallbacks(std::shared_ptr<V4l2DriverCb> cb) {
	VIDC_MED("V4l2Driver::registerCallbacks\n");
	mCb = cb;
	return 0;
}
