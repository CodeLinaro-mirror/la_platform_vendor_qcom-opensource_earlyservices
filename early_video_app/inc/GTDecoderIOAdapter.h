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
		GTDecoderIOAdapter(const char* sourceFilePath, const char* sourceConfigFilePath, const char* displayCard);
		bool isInputAvailabe();
        bool isInputEOS();
		int getInput(InputData* dstData);
		void releaseInput(InputData* sourceData);
		void setOutputFormat(struct v4l2_format* outputFormat);
		void setOutputImgResolution(int imgWidth, int imgHeight);
		void setOutputFrameRate(float frameRate);
		bool onOutput(std::uint8_t* pBuffer, uint32_t length, uint32_t offset, bool isKeyFrame);
        void close();
	private:
		struct v4l2_format* mOutputFormat = NULL;
		int mOutputImgWidth = 0;
		int mOutputImgHeight = 0;
		float mOutputFrameRate = 30; //default 30fps.

		FILE* mSourceInputFile = NULL;
		FILE* mSourceInputConfigFile = NULL;
		const char* mDisplayCard = NULL;
		FILE* mOutputFile = NULL;
		std::shared_ptr<DisplayAdaptor> mDisplayAdaptor = NULL;
		std::vector<uint64_t> mSourceFrameLengthVector;
        bool mIsInputEOS = false;
		int mCurrentInputDataIndex = 0;
};

#endif
