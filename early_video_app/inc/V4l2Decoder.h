/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef __MSM_V4L2_DECODER_H__
#define __MSM_V4L2_DECODER_H__

#include <list>
#include <mutex>

#include "V4l2Codec.h"
#include "V4l2CodecCallback.h"
#include "V4l2Driver.h"
#include "VidcLog.h"

using namespace early_video_app;

class V4l2Decoder : public V4l2Codec {
public:
    ~V4l2Decoder() {
        VIDC_MED("V4l2Decoder, destructor\n");
    };

    int init(unsigned int codec);
    void deinit();
    int configureInput();
    int configureOutput();
    int detectBitDepthChange();
    int detectFilmGrainChange(bool *hasFilmGrainChanged);
    int detectResolutionChange(bool *hasResolutionChanged);
    int reconfigureOutput();
    int registerCallbacks(std::shared_ptr<V4l2CodecCb> cb);
    int setOperatingRate(unsigned int numer, unsigned int denom);
    int setFrameRate(unsigned int numer, unsigned int denom);
    int setDSResolution(unsigned int width, unsigned int height);
    int getOperatingRate();
    int getFrameRate();
    int drain();
    int resume();
    int getFenceFds(struct V4L2OutputFenceInfo* fenceInfo);
    int getFenceFd(int fence_id);
};

#endif
