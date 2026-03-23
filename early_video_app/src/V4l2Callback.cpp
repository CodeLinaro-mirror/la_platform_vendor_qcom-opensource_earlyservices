/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include "../inc/V4l2Callback.h"

using namespace early_video_app;

V4l2Callback::V4l2Callback(V4l2Decoder* dec) : mV4l2Codec(dec) {
	VIDC_MED("V4l2Callback, constructor\n");
}

V4l2Callback::~V4l2Callback() {
	VIDC_MED("V4l2Callback, destructor\n");
}

int V4l2Callback::onV4l2BufferDone(struct v4l2_buffer* buffer) {
	if (mV4l2Codec->mCb->onBufferDone(buffer))
		return -EINVAL;
	return 0;
}

void V4l2Callback::onV4l2EventDone(struct v4l2_event* event) {
	mV4l2Codec->mCb->onEventDone(event);
}

int V4l2Callback::onV4l2Error(int error) {
	VIDC_ERR("V4l2Callback::onV4l2Error, %d\n", error);
	if (error == 0) {
		return 0;
	}
	int rc = mV4l2Codec->mCb->onError(error);
	if (rc != 0) {
		rc = -EINVAL;
	}
	return rc;
}


