/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef __GT_DECODER_IO_ADAPTER_H__
#define __GT_DECODER_IO_ADAPTER_H__

#include <sys/mman.h>
#include <list>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdio.h>
#include <chrono>
#include <memory>

#include "VideoDefines.h"
#include "../render/DisplayAdaptor.h"

class GTDecoderIOAdapter {
	public:
		bool isInputAvailabe();
        bool isInputEOS();
		bool getInput(InputData* dstData);
		void releaseInput(InputData* sourceData);
		void setOutputFormat(struct v4l2_format* outputForma);
		void setOutputFrameRate(float frameRate);
		bool onOutput(std::uint8_t* pBuffer, uint32_t length, uint32_t offset, bool isKeyFrame);
        void close();
	private:
        static void str2bytes(const std::string& source, InputData* data);
		static unsigned char hexStrToByte(const std::string& sourceHexStr);

		struct v4l2_format* mOutputFormat = NULL;
		float mOutputFrameRate = 30; //default 30fps.

		FILE* mOutputFile = NULL;
		std::shared_ptr<DisplayAdaptor> mDisplayAdaptor = NULL;
		int mCurrentInputDataIndex = 0;
        bool mIsInputEOS = false;
};

#endif
