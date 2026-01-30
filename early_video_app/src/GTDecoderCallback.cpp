/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include "../inc/GTDecoderCallback.h"

using namespace early_video_app;

GTDecoderCallback::GTDecoderCallback(GTDecoder* dec) :
	mGTDecoder(dec) {
	VIDC_HIGH("V4l2Callback, constructor\n");
}

GTDecoderCallback::~GTDecoderCallback() {
	VIDC_HIGH("V4l2Callback, destructor\n");
}

int GTDecoderCallback::onBufferDone(struct v4l2_buffer* buffer) {
	bool found = false;
	unsigned int index = 0xFFFFFFF;
	std::list<std::shared_ptr<v4l2_buffer>>::iterator it;
	std::unique_lock<std::mutex> lock(mGTDecoder->mV4l2Codec->mBufLock);
	if (buffer->type == INPUT_MPLANE) {
		VIDC_MED("GTDecoderCallback::onBufferDone, INPUT_MPLANE\n");
		for (std::list<std::shared_ptr<v4l2_buffer>>::iterator it =
				mGTDecoder->mV4l2Codec->mPendingInputBufs.begin();
			it != mGTDecoder->mV4l2Codec->mPendingInputBufs.end(); ++it) {
			if (buffer->index == (*it)->index) {
				found = true;
				index = buffer->index;
				print_v4l2_buffer("DQBUF DONE", buffer);
				mGTDecoder->mV4l2Codec->mInputBufs.push_back(*it);
				mGTDecoder->mV4l2Codec->mPendingInputBufs.remove(*it);
				break;
			}
		}
	} else if (buffer->type == OUTPUT_MPLANE) {
		VIDC_MED("GTDecoderCallback::onBufferDone, OUTPUT_MPLANE\n");
		for (std::list<std::shared_ptr<v4l2_buffer>>::iterator it = mGTDecoder->mV4l2Codec->mPendingOutputBufs.begin();
			it != mGTDecoder->mV4l2Codec->mPendingOutputBufs.end(); ++it) {
			if (buffer->index == (*it)->index) {
				found = true;

				if (mGTDecoder->mGTDecoderIOAdapter) {
					std::uint8_t *pBuffer = (std::uint8_t *)mmap(NULL, buffer->m.planes[0].length,
						PROT_READ, MAP_SHARED, buffer->m.planes[0].m.fd, 0);
					if (pBuffer == MAP_FAILED) {
						VIDC_ERR("GTDecoderCallback::onBufferDone, mmap failed, not dumping\n");
					} else {
						VIDC_MED("GTDecoderCallback::onBufferDone, output data, buffer->m.planes[0].bytesused = %d\n", buffer->m.planes[0].bytesused);
						mGTDecoder->mGTDecoderIOAdapter->onOutput(pBuffer, buffer->m.planes[0].bytesused);
						munmap((void *)pBuffer, buffer->m.planes[0].length);
					}
				}

				/* close fence fd for corresponding output buffer */
				bool foundFenceInfo = false;
				if (mGTDecoder->mV4l2Codec->isOutBufFenceEnabled()) {
					if (!mGTDecoder->mV4l2Codec->mOutBufFenceList.empty()) {
						for (std::list<std::shared_ptr<struct V4L2OutputFenceInfo>>::iterator itr =
							mGTDecoder->mV4l2Codec->mOutBufFenceList.begin();
							itr != mGTDecoder->mV4l2Codec->mOutBufFenceList.end(); ++itr) {
							if (buffer->index == OutputTag::toId((*itr)->outputBufTag)) {
								foundFenceInfo = true;
								for (int fd : (*itr)->fenceFds) {
									if (fd >= 0) {
										if (fd == 0)
											VIDC_ERR("GTDecoderCallback::onBufferDone, attempting to close fd 0\n");
										close(fd);
										VIDC_MED("GTDecoderCallback::onBufferDone, closed fence fd %d for buf %u\n",
											fd, buffer->index);
									} else {
										VIDC_ERR("GTDecoderCallback::onBufferDone, invalid fence fd for buf %u\n",
											buffer->index);
										/*
										* This is not a error case, as FBD for this corresponding
										* buffer might have already arrived from fw and corresponding
										* fence would have been signalled and destroyed.
										*/
									}
								}
								mGTDecoder->mV4l2Codec->mOutBufFenceList.remove(*itr);
								break;
							}
						}
					}
				}
				print_v4l2_buffer("DQBUF DONE", buffer);
				mGTDecoder->mFBDcount++;
				mGTDecoder->addFrameStats(buffer->m.planes[0].bytesused);
				/* if last flag event not enabled, driver sends last flag info via FBDs */
				if ((buffer->flags & V4L2_BUF_FLAG_LAST) && !mGTDecoder->mLastFlagEventEnabled) {
					buffer->flags &= ~V4L2_BUF_FLAG_LAST;
					if (mGTDecoder->mReconfigEventReceived) {
						mGTDecoder->mDrcLastFlagReceived = true;
						VIDC_HIGH("GTDecoderCallback::onBufferDone, drc last flag received\n");
					} else if (mGTDecoder->mDrainSent) {
						mGTDecoder->mDrainLastFlagReceived = true;
						VIDC_HIGH("GTDecoderCallback::onBufferDone, drain last flag received\n");
					}
				}
				if (mGTDecoder->mV4l2Codec->isOutBufFenceEnabled()) {
					if (buffer->m.planes[0].bytesused > 0 && !foundFenceInfo) {
						/*
						* mOutBufFenceList is supposed to have fence info for
						* all buffers except zero-filled length buffers
						*/
						VIDC_ERR("GTDecoderCallback::onBufferDone, output buffer fence info is missing\n");
						mGTDecoder->setFenceErrorCount();
					}
				}
				if (mGTDecoder->mV4l2Codec->mOutputStreamonDone) {
					if (mGTDecoder->mIsReallocateOutputBufferEnabled) {
						int size = (*it)->m.planes[0].length;
						int fd = (*it)->m.planes[0].m.fd;
						(*it)->m.planes[0].m.fd = mGTDecoder->mV4l2Codec->ionAlloc(size);
						mGTDecoder->mV4l2Codec->ionFree(fd);
					}
					mGTDecoder->mV4l2Codec->mOutputBufs.push_back(*it);
					mGTDecoder->mV4l2Codec->mPendingOutputBufs.remove(*it);
				}
				break;
			}
		}
	} else if (buffer->type == INPUT_META_PLANE) {
		VIDC_MED("GTDecoderCallback::onBufferDone, INPUT_META_PLANE\n");
		for (std::list<std::shared_ptr<v4l2_buffer>>::iterator it =
				mGTDecoder->mV4l2Codec->mPendingMetaInputBufs.begin();
			it != mGTDecoder->mV4l2Codec->mPendingMetaInputBufs.end(); ++it) {
			if (buffer->index == (*it)->index) {
				found = true;
				print_v4l2_buffer("DQBUF DONE", buffer);
				mGTDecoder->mV4l2Codec->mMetaInputBufs.push_back(*it);
				mGTDecoder->mV4l2Codec->mPendingMetaInputBufs.remove(*it);
				if (mGTDecoder->mV4l2Codec->isOutBufFenceEnabled()) {
					auto fenceIdInfo = std::make_shared<struct V4L2OutputFenceInfo>();
					/* Extract fence id, output buffer tag info, and get fence fd */
					auto inputMetaBuf = *it;
					mGTDecoder->mV4l2Codec->extractMetadata(inputMetaBuf.get(), fenceIdInfo.get());
					if (fenceIdInfo->outputBufTag == INVALID_VALUE || fenceIdInfo->fenceIds.empty()) {
						VIDC_ERR("GTDecoderCallback::onBufferDone, INPUT_META: failed to fetch output buffer tag or fence id\n");
						/*
						* codec config buffer will not have buffer tag metadata as there would be
						* no output buffer generated for this buffer. Hence, only in this case,
						* invalid outbuffer tag/fence id is accepted.
						* For other genuine error cases, mOutBufFenceList wont have fence fd info,
						* and treated as error when actual FBD is received.
						*/
						return 0;
					}
					if (mGTDecoder->mV4l2Codec->getFenceFds(fenceIdInfo.get())) {
						VIDC_ERR("GTDecoderCallback::onBufferDone, failed to get fence fd for fence id\n");
					}
					/* store fence infos in mOutBufFenceList */
					mGTDecoder->mV4l2Codec->mOutBufFenceList.push_back(fenceIdInfo);
				}
				break;
			}
		}
	} else if (buffer->type == OUTPUT_META_PLANE) {
		VIDC_MED("GTDecoderCallback::onBufferDone, OUTPUT_META_PLANE\n");
		for (std::list<std::shared_ptr<v4l2_buffer>>::iterator it =
				mGTDecoder->mV4l2Codec->mPendingMetaOutputBufs.begin();
			it != mGTDecoder->mV4l2Codec->mPendingMetaOutputBufs.end(); ++it) {
			if (buffer->index == (*it)->index) {
				found = true;
				print_v4l2_buffer("DQBUF DONE", buffer);
				mGTDecoder->mV4l2Codec->extractMetadata(buffer, nullptr);
				if ((buffer->flags & V4L2_BUF_FLAG_LAST) && !mGTDecoder->mLastFlagEventEnabled)
					buffer->flags &= ~V4L2_BUF_FLAG_LAST;
				if (mGTDecoder->mV4l2Codec->mOutputStreamonDone) {
					mGTDecoder->mV4l2Codec->mMetaOutputBufs.push_back(*it);
					mGTDecoder->mV4l2Codec->mPendingMetaOutputBufs.remove(*it);
				}
				break;
			}
		}
	} else {
		VIDC_ERR("GTDecoderCallback::onBufferDone, invalid buffer type %d\n", buffer->type);
		return -EINVAL;
	}
	if (!found)
		print_v4l2_buffer("Not Found", buffer);
	return 0;
}

void GTDecoderCallback::onEventDone(struct v4l2_event* event) {
	(void)event;
	if (event == nullptr) {
		VIDC_ERR("GTDecoderCallback::onEventDone, event is null\n");
		return;
	}
	VIDC_MED("GTDecoderCallback::onEventDone\n");
	if (event->type == V4L2_EVENT_SOURCE_CHANGE &&
		event->u.src_change.changes == V4L2_EVENT_SRC_CH_RESOLUTION) {
    	printKPILog("%s%s%s", LogKPITag, LogAPPTag, "source configuration changed");
		VIDC_HIGH("GTDecoderCallback::onEventDone, source change event received\n");
		mGTDecoder->mReconfigEventReceived = true;
		mGTDecoder->mV4l2Codec->mFirstReconfigReceived = true;
	}
	/* if last flag event is enabled, driver sends last flag info via event */
	if (event->type == V4L2_EVENT_EOS && mGTDecoder->mLastFlagEventEnabled) {
		if (mGTDecoder->mReconfigEventReceived && !mGTDecoder->mDrcLastFlagReceived) {
			mGTDecoder->mDrcLastFlagReceived = true;
			VIDC_HIGH("GTDecoderCallback::onEventDone Drc last flag event received\n");
		} else if (mGTDecoder->mDrainSent && !mGTDecoder->mDrainLastFlagReceived) {
			mGTDecoder->mDrainLastFlagReceived = true;
			VIDC_HIGH("GTDecoderCallback::onEventDone, Drain last flag event received\n");
		} else {
			VIDC_ERR("GTDecoderCallback::onEventDone, unexpected last flag event\n");
		}
	}
}

int GTDecoderCallback::onError(int error) {
	VIDC_ERR("GTDecoderCallback::onError\n");
	mGTDecoder->mErrorReceived = true;
	if (error) {
		VIDC_ERR("GTDecoderCallback::onError, %d\n", error);
		return -EINVAL;
	}
	return 0;
}