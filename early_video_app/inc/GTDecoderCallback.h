/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef __GT_DECODER_CALLBACK_H__
#define __GT_DECODER_CALLBACK_H__

#include <sys/mman.h>
#include <list>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <chrono>

#include "GTCodec.h"
#include "GTDecoder.h"
#include "V4l2Codec.h"
#include "V4l2CodecCallback.h"
#include "VideoDefines.h"

class GTDecoder;

class GTDecoderCallback : public V4l2CodecCb {
	public:
		GTDecoderCallback(GTDecoder* dec);
		~GTDecoderCallback();

		int onBufferDone(struct v4l2_buffer* buffer);
		void onEventDone(struct v4l2_event* event);
		int onError(int error);
	private:
		GTDecoder* mGTDecoder;
};

#endif
