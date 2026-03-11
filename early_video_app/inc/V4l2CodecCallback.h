
/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef __MSM_V4L2_CODEC_CALLBACK_H__
#define __MSM_V4L2_CODEC_CALLBACK_H__

#include <list>
#include "V4l2Driver.h"
#include <mutex>

#include "VidcLog.h"

class V4l2CodecCb {
    public:
        virtual ~V4l2CodecCb() = default;
        virtual int onBufferDone(struct v4l2_buffer* buffer) = 0;
        virtual void onEventDone(struct v4l2_event* event) = 0;
        virtual int onError(int error) = 0;
};

#endif
