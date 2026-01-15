/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef __GT_DECODER_H__
#define __GT_DECODER_H__

#include <sys/mman.h>
#include <list>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <chrono>

#include "V4l2Codec.h"
#include "EventHandler.h"
#include "VideoDefines.h"

#include "GTCodec.h"
#include "V4l2Decoder.h"
#include "GTDecoderIOAdapter.h"
#include "GTDecoderCallback.h"
#include "VidcLog.h"

class GTDecoderCallback;

class GTDecoder : public GTCodec {
	public:
		GTDecoder(unsigned int codec, unsigned int colorFmt);
		~GTDecoder();

		static int runDecoder();

		int gtRegisterCallbacks();
		void handleSeek(int seekTo);
		int queueBuffers();
		void queueAllBuffers(unsigned int totalFrames);
		int setControl(unsigned int ctrlId, int value);
		int setDSResolution(unsigned int downscaleWidth, unsigned int downscaleHeight);
		std::shared_ptr<EventHandler> getEventHandler();
		int setGTInputSizeOverWrite(int size);
		int setGTInputActualCount(int count);
		int setGTOutputActualCount(int count);
		int setSeekInfo(const int& seekFrom, const int& seekTo);
		int enableInputQbufSleep(int sleep);
		int setGTOutputBufferRecycle(bool enable);
		int setGTReallocateOutputBuffers(bool enable);
		void setFenceErrorCount();
		int getFenceErrorCount();

	private:
		friend class GTDecoderCallback;
		std::shared_ptr<GTDecoderCallback> mCb;
		std::shared_ptr<EventHandler> mEventHandler;
		std::shared_ptr<GTDecoderIOAdapter> mGTDecoderIOAdapter = nullptr;

		int mSeekFrom = -1;
		int mSeekTo = -1;
		bool inputQbufSleep = 0;
		int mFenceErrorCount = 0;
		bool mDrcLastFlagReceived = false;
		bool mIsReallocateOutputBufferEnabled = false;
};

#endif
