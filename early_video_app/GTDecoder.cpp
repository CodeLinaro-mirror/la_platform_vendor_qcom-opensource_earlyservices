/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include <asm-generic/errno-base.h>
#include <cstddef>
#include <sys/syscall.h>
#include <linux/module.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <chrono>
#include <filesystem>
#include <cstdio>

#include "../inc/GTDecoder.h"
#include "../inc/VidcLog.h"
#include "../inc/GTDecoderIOAdapter.h"
#include "../inc/VideoDefines.h"
#include "../driver/stub_driver/linux/inc/uapi/vidc/media//v4l2_vidc_extensions.h"

using namespace early_video_app;

void ThreadFunc(GTCodec& codec) {
	codec.threadLoop();
}

int GTDecoder::runDecoder(const char* inputFilePath, const char* inputConfigFilePath, const char* displayCard) {
    VIDC_HIGH("GTDecoder::runDecoder\n");
	if (inputFilePath == NULL || inputConfigFilePath == NULL || displayCard == NULL) {
        VIDC_ERR("GTDecoder::runDecoder, key params illegal!\n");
		return -EINVAL;
	}

	unsigned int colorFmt = V4L2_PIX_FMT_QC08C;//V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_QC08C;
    std::shared_ptr<GTDecoder> decoder = std::make_shared<GTDecoder>(V4L2_PIX_FMT_H264, colorFmt);
    if (decoder == NULL) {
        VIDC_ERR("GTDecoder::runDecoder, Decoder create failed!\n");
        return -ENOMEM;
    }
    else {
        VIDC_MED("GTDecoder::runDecoder, Decoder created successfully\n");
    }
	decoder->setSouce(inputFilePath, inputConfigFilePath);
	decoder->setDisplayCard(displayCard);

	/*create event thread*/
    std::shared_ptr<EventHandler> eventHandler = decoder->getEventHandler();
    int result = eventHandler->createEventThread();
    if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, event thread create failed!, result = %d\n", result);
        return -ENOMEM;
    }
    else {
        VIDC_MED("GTDecoder::runDecoder, event thread create successfully\n");
    }

	result = decoder->gtRegisterCallbacks();
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, register callback failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, register callback successfully\n");
    }

    result = decoder->gtCodecInit();
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, codec init failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, codec init successfully\n");
    }

	result = decoder->gtCodecSetColorFmt();
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, set color format failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, set color format successfully\n");
    }

	result = decoder->setControl(V4L2_CID_MPEG_VIDC_MIN_BITSTREAM_SIZE_OVERWRITE, 4);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, set min bitstream size overwrite failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, set min bitstream size overwrite successfully\n");
    }

	result = decoder->setGTInputSizeOverWrite(2 * 1024 * 1024);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, set input size overwrite failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, set input size overwrite successfully\n");
    }

	result = decoder->setControl(V4L2_CID_MPEG_VIDC_LAST_FLAG_EVENT_ENABLE, 1);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, set last flag enable control failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, set last flag enable control successfully\n");
    }

	result = decoder->setGTInputActualCount(64);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, set input actual count failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, set input actual count successfully\n");
    }

	result = decoder->setGTOutputActualCount(64);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, set actual output count failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, set actual output count successfully\n");
    }

	result = eventHandler->queueEvent(EVENT_CONFIGURE_INPUT, true);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, queue configure input event failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, queue configure input event successfully\n");
    }

	result = eventHandler->queueEvent(EVENT_CONFIGURE_OUTPUT, true);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, queue configure output event failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, queue configure output event successfully\n");
    }

	result = decoder->setControl(V4L2_CID_MPEG_VIDC_PRIORITY, 0);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, set priority control event failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, set priority control event successfully\n");
    }

	result = eventHandler->queueEvent(EVENT_START_INPUT, true);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, start input failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, start input successfully\n");
    }

	result = decoder->allocateBuffers(INPUT_PORT);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, allocate input buffer failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, allocate input buffer successfully\n");
    }

	result = decoder->queueBuffers();
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, queue buffer failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, queue buffer successfully\n");
    }

	result = eventHandler->queueEvent(EVENT_STOP_OUTPUT, true);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, stop output failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, stop output successfully\n");
    }

	result = eventHandler->queueEvent(EVENT_STOP_INPUT, true);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, stop input failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, stop input successfully\n");
    }

	decoder->freeBuffers(OUTPUT_PORT);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, free output buffer failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, free output buffer successfully\n");
    }

	decoder->freeBuffers(INPUT_PORT);
	if (result != 0) {
        VIDC_ERR("GTDecoder::runDecoder, free input buffer failed!, result = %d\n", result);
		return result;
	}
    else {
        VIDC_MED("GTDecoder::runDecoder, free input buffer successfully\n");
    }

	return result;
}

GTDecoder::GTDecoder(unsigned int codec, unsigned int colorFmt) :
	GTCodec(codec, colorFmt) {
	VIDC_MED("GTDecoder, Constructor\n");
	mV4l2Codec = std::make_shared<V4l2Decoder>();
	mEventHandler = std::make_shared<EventHandler>(mV4l2Codec);
	mCb = std::make_shared<GTDecoderCallback>(this);
}

GTDecoder::~GTDecoder() {
	VIDC_MED("GTDecoder, Destructor\n");
	if (mGTDecoderIOAdapter != NULL) {
		mGTDecoderIOAdapter->close();
		mGTDecoderIOAdapter = NULL;
	}
	if (mEventHandler != NULL) {
		mEventHandler->stopEventThread();
		mEventHandler = NULL;
	}
	if (mV4l2Codec != NULL) {
		mV4l2Codec->deinit();
		mV4l2Codec = NULL;
	}
}

void GTDecoder::setSouce(const char* inputFilePath, const char* inputConfigFilePath) {
	mSourceFilePath = inputFilePath;
	mSourceConfigFilePath = inputConfigFilePath;
}

void GTDecoder::setDisplayCard(const char* displayCard) {
	mDisplayCard = displayCard;
}

std::shared_ptr<EventHandler> GTDecoder::getEventHandler() {
	return mEventHandler;
}

int GTDecoder::setGTInputSizeOverWrite(int size) {
	return mV4l2Codec->setInputSizeOverWrite(size);
}

int GTDecoder::setGTInputActualCount(int count) {
	return mV4l2Codec->setInputActualCount(count);
}

int GTDecoder::setGTOutputActualCount(int count) {
	return mV4l2Codec->setOutputActualCount(count);
}

int GTDecoder::setSeekInfo(const int& seekFrom, const int& seekTo) {
	mSeekFrom = seekFrom;
	mSeekTo = seekTo;
	return 0;
}

int GTDecoder::enableInputQbufSleep(int sleep) {
	inputQbufSleep = sleep;
	return 0;
}

int GTDecoder::setGTOutputBufferRecycle(bool enable) {
	return mV4l2Codec->setOutputBufferRecycle(enable);
}

int GTDecoder::setGTReallocateOutputBuffers(bool enable) {
	mIsReallocateOutputBufferEnabled = enable;
	return 0;
}

void GTDecoder::setFenceErrorCount() {
	mFenceErrorCount++;
	VIDC_MED("GTDecoder::setFenceErrorCount, %u\n", mFenceErrorCount);
}

int GTDecoder::getFenceErrorCount() {
	return mFenceErrorCount;
}

int GTDecoder::gtRegisterCallbacks() {
	return mV4l2Codec->registerCallbacks(mCb);
}

int GTDecoder::setControl(unsigned int ctrlId, int value) {
	int ret = mV4l2Codec->setControl(ctrlId, value);
	if (ret) {
		VIDC_ERR("GTDecoder::setControl, failed, ret = %d\n", ret);
		return ret;
	}
	if (ctrlId == V4L2_CID_MPEG_VIDC_LAST_FLAG_EVENT_ENABLE && value)
		setLastFlagEvent(value);
	return 0;
}

int GTDecoder::setDSResolution(unsigned int downscaleWidth, unsigned int downscaleHeight) {
	int ret = mV4l2Codec->setDSResolution(downscaleWidth, downscaleHeight);
	if (ret) {
		VIDC_ERR("GTDecoder::setDSResolution, Set Downscale resolution failed\n");
		return ret;
	}
	return 0;
}

void GTDecoder::handleSeek(int seekTo) {
	int ret = mEventHandler->queueEvent(EVENT_STOP_INPUT, true);
	if (ret) {
		VIDC_ERR("GTDecoder::handleSeek, stop input failed, result = %d\n", ret);
		return;
	}

	ret = mEventHandler->queueEvent(EVENT_STOP_META_INPUT, true);
	if (ret) {
		VIDC_ERR("GTDecoder::handleSeek, stop meta input failed, result = %d\n", ret);
		return;
	}

	ret = mEventHandler->queueEvent(EVENT_START_META_INPUT, true);
	if (ret) {
		VIDC_ERR("GTDecoder::handleSeek, start meta input failed, result = %d\n", ret);
		return;
	}

	ret = mEventHandler->queueEvent(EVENT_START_INPUT, true);
	if (ret) {
		VIDC_ERR("GTDecoder::handleSeek, start input failed, result = %d\n", ret);
	}
}

int GTDecoder::queueBuffers() {
	VIDC_MED("GTDecoder::queueBuffers: enter\n");

	int ret = 0;
	bool eos = false;
	int frameCounter = 0;
	char vidc_gdsc_enable = 0;
	char found = 0;

	while (1) {
		VIDC_MED("GTDecoder::queueBuffers: loop\n");
		bool inputAvailable = false;
		std::shared_ptr<v4l2_buffer> metaInput = nullptr;
		std::shared_ptr<v4l2_buffer> metaOutput = nullptr;
		std::shared_ptr<v4l2_buffer> Input = nullptr;
		std::shared_ptr<v4l2_buffer> Output = nullptr;

		if (mErrorReceived.load()) {
			VIDC_ERR("GTDecoder::queueBuffers: received error\n");
			break;
		}

		/*
		 * for partial reconfigure, do not wait for last flag
		 * for full reconfigure, wait for event and last flag
		 */
		if (!mV4l2Codec->mOutputStreamonDone) {
			if (mReconfigEventReceived.load()) {
				mReconfigEventReceived.store(false);
				VIDC_MED("GTDecoder::queueBuffers: src change event arrived\n");
				ret = mEventHandler->queueEvent(EVENT_CONFIGURE_OUTPUT, true);

				if (ret) {
					VIDC_ERR("GTDecoder::queueBuffers, src configuration failed\n");
					return -EINVAL;
				}

				ret = mV4l2Codec->allocateMetaBuffers(OUTPUT_META_PORT);
				if (ret) {
					VIDC_ERR("GTDecoder::queueBuffers, allocation of output meta buffer failed\n");
					return -EINVAL;
				}

				ret = mV4l2Codec->allocateBuffers(OUTPUT_PORT);
				if (ret) {
					VIDC_ERR("GTDecoder::queueBuffers, allocation of output buffer failed\n");
					return -EINVAL;
				}

				ret = mEventHandler->queueEvent(EVENT_START_META_OUTPUT, true);
				if (ret) {
					VIDC_ERR("GTDecoder::queueBuffers, output meta port start failed\n");
					return -EINVAL;
				}

				ret = mEventHandler->queueEvent(EVENT_START_OUTPUT, true);
				if (ret) {
					VIDC_ERR("GTDecoder::queueBuffers, output port start failed\n");
					return -EINVAL;
				}
			}
		}
		if (mV4l2Codec->mOutputStreamonDone && mReconfigEventReceived.load()) {
			if (mDrcLastFlagReceived.load()) {
				mDrcLastFlagReceived.store(false);
				mReconfigEventReceived.store(false);
				VIDC_MED("GTDecoder::queueBuffers, last flag for reconfig arrived\n");
				ret = mEventHandler->queueEvent(EVENT_RECONFIGURE, true);
				if (ret) {
					VIDC_ERR("GTDecoder::queueBuffers, event reconfiguration failed\n");
					return -EINVAL;
				}
			}
		}

		if (mV4l2Codec->mOutputStreamonDone) {
			bool outputMetadataEnabled = mV4l2Codec->isOutputMetadataEnabled();
			while (1) {
				bool found = false;
				{
					std::unique_lock<std::mutex> lock(mV4l2Codec->mOutputBufLock);
					if (mIsQueueSingleBufferEnabled.load()) {
						if (mV4l2Codec->mPendingOutputBufs.size() == 1) {
							break;
						}
					} else if (mV4l2Codec->getMinOutputCount() <= mV4l2Codec->mPendingOutputBufs.size()) {
						break;
					}

					Output = mV4l2Codec->mOutputBufs.front();
					if (Output == nullptr) {
						VIDC_ERR("GTDecoder::queueBuffers, output buffer is null\n");
						return -EINVAL;
					}

					if (outputMetadataEnabled) {
						for (auto it = mV4l2Codec->mMetaOutputBufs.begin();
							it != mV4l2Codec->mMetaOutputBufs.end(); ++it) {
							metaOutput = *it;
							if (metaOutput == nullptr) {
								VIDC_ERR("GTDecoder::queueBuffers, meta output buffer is null\n");
								return -EINVAL;
							}
							if (Output->index == metaOutput->index) {
								found = true;
								break;
							}
						}
						if (!found) {
						    VIDC_ERR("GTDecoder::queueBuffers, meta buffer not found for o/p index %d\n", Output->index);
							return -EINVAL;
						}
					}
					else {
						found = true;
					}
				}
				if (found) {
					{
						std::unique_lock<std::mutex> lock(mV4l2Codec->mOutputBufLock);

						if (outputMetadataEnabled) {
							mV4l2Codec->mMetaOutputBufs.remove(metaOutput);
							mV4l2Codec->mPendingMetaOutputBufs.push_back(metaOutput);
						}
						mV4l2Codec->mOutputBufs.pop_front();
						mV4l2Codec->mPendingOutputBufs.push_back(Output);
					}
					if (outputMetadataEnabled) {
						ret = mV4l2Codec->fillMetadata(metaOutput);
						if (ret) {
							VIDC_ERR("GTDecoder::queueBuffers, meta output buffer fill failed\n");
							return -EINVAL;
						}
						ret = mV4l2Codec->queueBuffer(metaOutput);
						if (ret) {
							VIDC_ERR("GTDecoder::queueBuffers, meta output buffer queue failed\n");
							return -EINVAL;
						}
					}
					ret = mV4l2Codec->queueBuffer(Output);
					if (ret) {
						VIDC_ERR("GTDecoder::queueBuffers, output buffer queue failed\n");
						return -EINVAL;
					}
				}
			}

			if (mDrainPending.load()) {
				mDrainPending.store(false);
				VIDC_MED("GTDecoder::queueBuffers, sending drain\n");
				mDrainSent.store(true);
				ret = mV4l2Codec->drain();
				if (ret) {
					VIDC_ERR("GTDecoder::queueBuffers, drain failed\n");
					return -EINVAL;
				}
			}
		}

		if (mDrainSent.load()) {
			if (mDrainLastFlagReceived.load()) {
				mDrainLastFlagReceived.store(false);
				mDrainSent.store(false);
				VIDC_MED("GTDecoder::queueBuffers, last flag for drain arrived\n");
				return 0;//here quit safely after all frame decoded and rendered.
			}
			else {
				usleep(10 * 1000);
				continue;
			}
		}

		{
			std::unique_lock<std::mutex> lock(mV4l2Codec->mInputBufLock);
			inputAvailable = !mV4l2Codec->mInputBufs.empty();
		}
		/* unlock and sleep */
		if (!inputAvailable) {
			usleep(10 * 1000);
			continue;
		}
		{
			bool inputMetadataEnabled = mV4l2Codec->isInputMetadataEnabled();
			bool found = false;

			std::unique_lock<std::mutex> lock(mV4l2Codec->mInputBufLock);
			Input = mV4l2Codec->mInputBufs.front();
			if (Input == nullptr) {
				VIDC_ERR("GTDecoder::queueBuffers, input buffer is null\n");
				return -EINVAL;
			}
			if (inputMetadataEnabled) {
				for (auto it = mV4l2Codec->mMetaInputBufs.begin();
					it != mV4l2Codec->mMetaInputBufs.end(); ++it) {
					metaInput = *it;
					if (metaInput == nullptr) {
						VIDC_ERR("GTDecoder::queueBuffers, input meta buffer is null\n");
						return -EINVAL;
					}
					if (Input->index == metaInput->index) {
						found = true;
						break;
					}
				}
				if (!found) {
				    VIDC_ERR("GTDecoder::queueBuffers, meta buffer not found for i/p index %d\n", Input->index);
					return -EINVAL;
				}
			}
			else {
				found = true;
			}

			if (found) {
				if (inputMetadataEnabled) {
					mV4l2Codec->mMetaInputBufs.remove(metaInput);
					mV4l2Codec->mPendingMetaInputBufs.push_back(metaInput);
				}

				mV4l2Codec->mInputBufs.pop_front();
				mV4l2Codec->mPendingInputBufs.push_back(Input);
			}
		}

		unsigned int isCSD = 0;
		auto buffer = Input.get();
		if (buffer == nullptr) {
			VIDC_ERR("GTDecoder::queueBuffers, buffer is null\n");
			return -EINVAL;
		}

		/*----------------fill data begin--------------*/
		VIDC_MED("GTDecoder::queueBuffers, Input raw data\n");
		usleep(inputQbufSleep * 1000);
		std::shared_ptr<Buffer> spBuffer = nullptr;
        if (!buffer->m.planes[0].data_offset) {
            spBuffer.reset(new Buffer(
                        buffer->m.planes[0].m.fd,
                        buffer->m.planes[0].length,
                        Type::LINEAR));
        }
		else if (buffer->m.planes[0].data_offset > 0) {
            spBuffer.reset(new Buffer(
                        buffer->m.planes[0].m.fd,
                        buffer->m.planes[0].data_offset,
                        buffer->m.planes[0].length - buffer->m.planes[0].data_offset,
                        Type::LINEAR));
        }
		auto mapping = spBuffer->map();
		if (mGTDecoderIOAdapter == nullptr) {
			mGTDecoderIOAdapter = std::make_shared<GTDecoderIOAdapter>(mSourceFilePath, mSourceConfigFilePath, mDisplayCard);
		}
		InputData inputData;
		int result = mGTDecoderIOAdapter->getInput(&inputData);
		if (result > 0) {
			memcpy(mapping->vaddr(), inputData.data, inputData.length);
			spBuffer->setFilledRange(0, inputData.length);
			spBuffer->setTimeStamp(inputData.frameIndex * 1000000lu);
			if (inputData.isCodecConfig) {
				uint32_t flags = spBuffer->flags() | Buffer::Flags::CODEC_CONFIG;
				spBuffer->setFlags((Buffer::Flags)flags);
			}
			if(inputData.isLastFrame) {
				uint32_t flags = spBuffer->flags() | Buffer::Flags::EOS;
				spBuffer->setFlags((Buffer::Flags)flags);
			}
			mGTDecoderIOAdapter->releaseInput(&inputData);
			VIDC_MED("GTDecoder::queueBuffers, input data length: %d, spBuffer->timestamp():%d, inputData.isCodecConfig = %d, inputData.isLastFrame = %d\n",
				inputData.length, spBuffer->timestamp(), inputData.isCodecConfig, inputData.isLastFrame);
		}
		else if (result == 0) {
			VIDC_HIGH("GTDecoder::queueBuffers, no more data got\n");
		}
		else {
			VIDC_HIGH("GTDecoder::queueBuffers, no valid input\n");
			return -EINVAL;
		}

		buffer->timestamp.tv_sec = static_cast<int64_t>(spBuffer->timestamp()) / 1000000ll;
		buffer->timestamp.tv_usec = static_cast<int64_t>(spBuffer->timestamp()) % 1000000ll;
		buffer->m.planes[0].bytesused = spBuffer->filledLength() + buffer->m.planes[0].data_offset;
		if (spBuffer->flags() & Buffer::Flags::CODEC_CONFIG) {
		    isCSD = true;
		}
		VIDC_MED("GTDecoder::queueBuffers, Input raw data end\n");
		/*----------------fill data end--------------*/

		VIDC_MED("GTDecoder::queueBuffers, buffer->m.planes[0].bytesused = %d, buffer->m.planes[0].data_offset = %d\n",
			buffer->m.planes[0].bytesused, buffer->m.planes[0].data_offset);
		if (buffer->m.planes[0].bytesused == buffer->m.planes[0].data_offset) {
			if (!mV4l2Codec->mOutputStreamonDone) {
				VIDC_MED("GTDecoder::queueBuffers, post pone drain\n");
				mDrainPending.store(true);
			}
			else {
				VIDC_HIGH("GTDecoder::queueBuffers, sending drain ..\n");
				mDrainSent.store(true);
				ret = mV4l2Codec->drain();
				if (ret) {
					VIDC_ERR("GTDecoder::queueBuffers, drain failed\n");
					return -EINVAL;
				}
			}
		}
		else {
			mV4l2Codec->setFrameCount(frameCounter);
			if (metaInput) {
				ret = mV4l2Codec->fillMetadata(metaInput);
				if (ret) {
					VIDC_ERR("GTDecoder::queueBuffers, fill metadata failed for meta input\n");
					return -EINVAL;
				}
				ret = mEventHandler->queueBuffer(EVENT_QBUF, metaInput);
				if (ret) {
					VIDC_ERR("GTDecoder::queueBuffers, queue buffer failed for meta input\n");
					return -EINVAL;
				}
			}
			if (isCSD) {
				ret = mV4l2Codec->setControl(V4L2_CID_MPEG_VIDC_CODEC_CONFIG, isCSD);
				if (ret) {
					VIDC_ERR("GTDecoder::queueBuffers, set control failed\n");
					return -EINVAL;
				}
			}
			ret = mEventHandler->queueBuffer(EVENT_QBUF, Input);
			if (ret) {
				VIDC_ERR("GTDecoder::queueBuffers, queue buffer failed for input\n");
				return -EINVAL;
			}
			VIDC_LOW("GTDecoder::queueBuffers, queue buffer for input\n");
		}
		frameCounter++;
		if (mIFPC) {
			/*
			 * This is driver dependent change.
			 * If driver changes time, it needs to change.
			 * For now, putting 10 seconds delay
			 */
			VIDC_MED("GTDecoder::queueBuffers, sleep for 10 seconds\n");
			usleep(10 * 1000 * 1000);
			VIDC_MED("GTDecoder::queueBuffers, check for driver power collapse\n");
			FILE *fp = fopen("/sys/kernel/debug/regulator/aaf804c.qcom,gdsc-video_cc_mvs0c_gdsc/enable", "r");
			if (fp != NULL) {
				fread(&vidc_gdsc_enable, sizeof(char), 1, fp);
				VIDC_MED("GTDecoder::queueBuffers, reg - GDSC enable = %c\n", vidc_gdsc_enable);
				fclose(fp);
			}
			else {
				fp = fopen("/sys/kernel/debug/pm_genpd/video_cc_mvs0c_gdsc/current_state", "r");
				if (fp != NULL) {
					while (fread(&found, sizeof(char), 1, fp) == 1) {
						if (found == '-') {
							fread(&vidc_gdsc_enable, sizeof(char), 1, fp);
							VIDC_MED("GTDecoder::queueBuffers, genPD - GDSC enable = %c\n", vidc_gdsc_enable);
							break;
						}
					}
					fclose(fp);
				}
				else {
					VIDC_ERR("GTDecoder::queueBuffers, error in getting gdsc status. could be missing mount fs.");
					return -1;
				}
			}
			if (vidc_gdsc_enable != '0') {
				VIDC_ERR("GTDecoder::queueBuffers, Driver not power collapsed\n");
				return -EINVAL;
			}
		}
		if (mSSRValueString != nullptr && mSSRatFrameNum > 0 && frameCounter == mSSRatFrameNum) {
			VIDC_ERR("GTDecoder::queueBuffers, Trigger SSR at %d th frame\n", mSSRatFrameNum);
			ret = gtCodecRunSSR();
			if (ret)
				return ret;
		}
		if (mStabilityValueString != nullptr && mHWResetAtFrameNum > 0 && frameCounter == mHWResetAtFrameNum) {
			VIDC_ERR("GTDecoder::queueBuffers, Trigger stability at %d th frame\n", mHWResetAtFrameNum);
			ret = gtCodecRunStabilityCmd();
			if (ret)
				return ret;
		}
		/* handle seek scenario */
		if (mSeekFrom == frameCounter) {
			/* keep 30 ms sleep before seek to ensure drain, ipsc and seek
			 * scenario is replicated. also do not add sleep at any other places
			 * in buffer queuing logic to avoid disturbing above scenario
			 */
			int count = 3;
#ifdef _LINUX_VENV_
			count = 500;
#endif
			while (!mV4l2Codec->mFirstReconfigReceived) {
				usleep(10 * 1000);
				count--;
				if (count < 0) {
					VIDC_ERR("GTDecoder::queueBuffers, Error:First reconfig frame taking too long\n");
					return -EINVAL;
				}
			}
			handleSeek(mSeekTo);
			frameCounter = mSeekTo;
			VIDC_MED("GTDecoder::queueBuffers, seek from frame %d to %d complete\n", mSeekFrom, mSeekTo);
			mSeekFrom = -1;
		}
	}

	if (mV4l2Codec->mV4l2Driver->mError) {
		VIDC_ERR("GTDecoder::queueBuffers, error occured in V4l2Driver\n");
		return -EINVAL;
	}

	return 0;
}