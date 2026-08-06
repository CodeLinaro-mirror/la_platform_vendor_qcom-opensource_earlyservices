/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <filesystem>
#include <cstdio>
#include <atomic>
#include <unistd.h>
#include <cerrno>

#include "../inc/GTDecoderIOAdapter.h"
#include "../inc/VidcLog.h"

using namespace early_video_app;

static const char* OutputCacheDir = "/vendor_early_services/run/early_video_app/VideoDecoder";
std::atomic<uint64_t> lastCommitFrameTimestamp{0};

GTDecoderIOAdapter::GTDecoderIOAdapter(const char* sourceFilePath, const char* sourceConfigFilePath, const char* displayCard) :
mDisplayCard(displayCard) {
	VIDC_HIGH("GTDecoderIOAdapter::GTDecoderIOAdapter, sourceFilePath = %s, sourceConfigFilePath = %s, displayCard = %s\n",
		(sourceFilePath == NULL ? "NULL" : sourceFilePath),
		(sourceConfigFilePath == NULL ? "NULL" : sourceConfigFilePath),
		(displayCard == NULL ? "NULL" : displayCard));
	if (sourceFilePath != NULL) {
		mSourceInputFile = fopen(sourceFilePath, "rb");
	}
	if (sourceConfigFilePath != NULL) {
		mSourceInputConfigFile = fopen(sourceConfigFilePath, "r");
	}
}

bool GTDecoderIOAdapter::isInputAvailabe() {
	return true;
}

bool GTDecoderIOAdapter::isInputEOS() {
	return mIsInputEOS;
}

int GTDecoderIOAdapter::getInput(InputData* dstData) {
	if (dstData == NULL) {
		VIDC_ERR("GTDecoderIOAdapter::getInput, dstData invalid\n");
		return -1;
	}
	if (mSourceInputFile == NULL || mSourceInputConfigFile == NULL) {
		VIDC_ERR("GTDecoderIOAdapter::getInput, source file or source config file invalid\n");
		return -1;
	}

	VIDC_MED("GTDecoderIOAdapter::getInput, enter\n");
	//read config file for data frame length
	if (mSourceFrameLengthVector.size() <= 0) {
		char* readLine = NULL;
		size_t lineLength = 0;
		int readResult = getline(&readLine, &lineLength, mSourceInputConfigFile);
		if (readResult == -1 || lineLength <= 0) {
			if (readLine != NULL) {
				free(readLine);
			}
			return -1;
		}
		std::string configStr = readLine;
		free(readLine);
		std::stringstream strStream(configStr);
		std::string item;
		while (std::getline(strStream, item, ',')) {
			char* end = NULL;
			errno = 0;
			uint64_t value = std::strtoull(item.c_str(), &end, 10);
			if (errno == ERANGE || end == item.c_str() || *end != '\0') {
				VIDC_HIGH("GTDecoderIOAdapter::getInput, not illegal frame length string \"%s\"\n", item.c_str());
			}
			else {
				mSourceFrameLengthVector.push_back(value);
			}
		}
		if (mSourceFrameLengthVector.size() <= 0) {
			VIDC_HIGH("GTDecoderIOAdapter::getInput, not found frame config in file\n");
			return -1;
		}
	}

	//read data frame
	if (mCurrentInputDataIndex >= mSourceFrameLengthVector.size()) {
		VIDC_HIGH("GTDecoderIOAdapter::getInput, index over end, index = %d, vector.size = %d\n",
			mCurrentInputDataIndex, mSourceFrameLengthVector.size());
		return 0;
	}
	uint64_t needReadLength = mSourceFrameLengthVector[mCurrentInputDataIndex];
	if (needReadLength <= 0) {
		VIDC_HIGH("GTDecoderIOAdapter::getInput, frame length not valid\n");
		return -1;
	}
	unsigned char* buffer = new unsigned char[needReadLength];
	int readLength = fread(buffer, 1, needReadLength * sizeof(unsigned char), mSourceInputFile);
	if (readLength <= 0) {
		mIsInputEOS = true;
		delete[] buffer;
		VIDC_HIGH("GTDecoderIOAdapter::getInput, reach file end\n");
		if (mCurrentInputDataIndex > 0) {
			return 0;
		}
		else {
			return -1;
		}
	}
	if (mCurrentInputDataIndex <= 0) {
		dstData->isCodecConfig = true;
	}
	else if (mCurrentInputDataIndex == (mSourceFrameLengthVector.size() - 1)) {
		dstData->isLastFrame = true;
	}
	dstData->data = buffer;
	dstData->length = readLength;
	VIDC_MED("GTDecoderIOAdapter::getInput, exit, mCurrentInputDataIndex = %d\n", mCurrentInputDataIndex);
	mCurrentInputDataIndex++;
	return true;
}

void GTDecoderIOAdapter::releaseInput(InputData* sourceData) {
	if (sourceData == NULL || sourceData->data == NULL) {
		return;
	}
	delete[] sourceData->data;
}

void GTDecoderIOAdapter::setOutputFormat(struct v4l2_format* outputFormat) {
	mOutputFormat = outputFormat;
}

void GTDecoderIOAdapter::setOutputImgResolution(int imgWidth, int imgHeight) {
	mOutputImgWidth = imgWidth;
	mOutputImgHeight = imgHeight;
	if (mOutputFormat != NULL) {
		if (mOutputImgWidth <= 0) {
			mOutputImgWidth = mOutputFormat->fmt.pix_mp.width;
		}
		if (mOutputImgHeight <= 0) {
			mOutputImgHeight = mOutputFormat->fmt.pix_mp.height;
		}
	}
}

void GTDecoderIOAdapter::setOutputFrameRate(float frameRate) {
	mOutputFrameRate = frameRate;
}

bool GTDecoderIOAdapter::onOutput(std::uint8_t* pBuffer, uint32_t length, uint32_t offset, bool isKeyFrame) {
	if (pBuffer == NULL || length <= 0 || offset >= length) {
		return false;
	}

	static bool isFirstKPILogPrinted = false;
	if (!isFirstKPILogPrinted) {
		isFirstKPILogPrinted = true;
		printKPILog("%s%s%s", LogKPITag, LogAPPTag, "1st video frame output");
	}
	VIDC_HIGH("GTDecoderIOAdapter::onOutput, video frame decoded, length = %d, offset = %d, isKeyFrame = %d\n", length, offset, isKeyFrame);

	bool result = false;
	if (mDisplayAdaptor == NULL) {
		mDisplayAdaptor = std::make_shared<DisplayAdaptor>();
	}
	if (!mDisplayAdaptor->isInitialized()) {
		int outputWidth = 0;
		int outputHeight = 0;
		if (mOutputFormat != NULL) {
			outputWidth = mOutputFormat->fmt.pix_mp.width;
			outputHeight = mOutputFormat->fmt.pix_mp.height;
		}
		bool result = mDisplayAdaptor->initAdaptor(mOutputImgWidth, mOutputImgHeight, outputWidth, outputHeight, length, mDisplayCard);
		if (!result) {
			mDisplayAdaptor->deinitAdaptor();
			VIDC_HIGH("GTDecoderIOAdapter::onOutput, display init failed!\n");
		}
	}
	if (mDisplayAdaptor != NULL && mDisplayAdaptor->isInitialized()) {
		//do render speed control, if audio brought in, also need do av sync here.
		if (mOutputFrameRate > 0) {
			struct timeval timeValue;
			gettimeofday(&timeValue, NULL);
			uint64_t timeNum = timeValue.tv_sec * 1000 + timeValue.tv_usec / 1000;
			if (lastCommitFrameTimestamp.load() <= 0) {
				lastCommitFrameTimestamp.store(timeNum);
				result = mDisplayAdaptor->commitData(pBuffer, length, offset);
			}
			else {
				uint64_t timeDelta = 0;
				if (timeNum >= lastCommitFrameTimestamp.load()) {
					timeDelta = timeNum - lastCommitFrameTimestamp.load();
				}
				VIDC_MED("GTDecoderIOAdapter::onOutput, timeDelta = %d\n", timeDelta);

				uint64_t perFrameTime = 1000 / mOutputFrameRate;
				if (timeDelta > perFrameTime && !isKeyFrame) {
					lastCommitFrameTimestamp.store(timeNum);
				}
				else {
					uint64_t waitTime = 0;
					if (perFrameTime > timeDelta) {
						waitTime = perFrameTime - timeDelta;
						usleep(waitTime * 1000);
					}
					gettimeofday(&timeValue, NULL);
					timeNum = timeValue.tv_sec * 1000 + timeValue.tv_usec / 1000;
					lastCommitFrameTimestamp.store(timeNum);
					result = mDisplayAdaptor->commitData(pBuffer, length, offset);
				}
			}
		}
		else {
			result = mDisplayAdaptor->commitData(pBuffer, length, offset);
		}
	}
#if 0
	if (mOutputFile == NULL) {
		auto time = std::time(nullptr);
		std::tm localTime = *std::localtime(&time);
		std::ostringstream osStrStream;
		osStrStream << std::put_time(&localTime, "%Y_%m_%d_%H_%M_%S");
		std::string currentDateTimeStr = osStrStream.str();
		std::filesystem::path decodedOutputCacheDir = OutputCacheDir;
		bool fileCheckResult = true;
		if (!std::filesystem::exists(decodedOutputCacheDir)) {
			fileCheckResult = std::filesystem::create_directories(decodedOutputCacheDir);
		}
		VIDC_MED("GTDecoderIOAdapter::onOutput, dump path check result : %d\n", fileCheckResult);
		if (fileCheckResult) {
			std::string decodedOutputCachePath = decodedOutputCacheDir.string() + "/" + currentDateTimeStr + ".yuv";
			VIDC_HIGH("GTDecoderIOAdapter::onOutput, dump path : %s\n", decodedOutputCachePath.c_str());
			mOutputFile = fopen(decodedOutputCachePath.c_str(), "w+b");
		}
	}
	if (mOutputFile != NULL) {
		VIDC_MED("GTDecoderIOAdapter::onOutput, output data wrote to file, length = %d\n", length);
		fwrite(pBuffer, length, 1, mOutputFile);
		result = true;
	}
#endif
	return result;
}

void GTDecoderIOAdapter::close() {
	if (mDisplayAdaptor != NULL) {
		mDisplayAdaptor->deinitAdaptor();
		mDisplayAdaptor = NULL;
	}
	if (mSourceInputFile != NULL) {
		fclose(mSourceInputFile);
		mSourceInputFile = NULL;
	}
	if (mSourceInputConfigFile != NULL) {
		fclose(mSourceInputConfigFile);
		mSourceInputConfigFile = NULL;
	}
	if (mOutputFile != NULL) {
		fclose(mOutputFile);
		mOutputFile = NULL;
	}
	mSourceFrameLengthVector.clear();
	mIsInputEOS = false;
	lastCommitFrameTimestamp.store(0);
	mOutputFormat = NULL;
	mDisplayCard = NULL;
}


