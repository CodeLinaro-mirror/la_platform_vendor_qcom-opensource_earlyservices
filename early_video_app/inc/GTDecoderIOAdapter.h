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

#include "VideoDefines.h"

class GTDecoderIOAdapter {
	public:
		bool isInputAvailabe();
        bool isInputEOS();
		bool getInput(InputData* dstData);
		void releaseInput(InputData* sourceData);
		bool onOutput(std::uint8_t* pBuffer, uint32_t length);
        void close();
	private:
        static void str2bytes(const std::string& source, InputData* data);
		static unsigned char hexStrToByte(const std::string& sourceHexStr);

		FILE* mOutputFile = NULL;
		int mCurrentInputDataIndex = 0;
        bool mIsInputEOS = false;
};

#endif
