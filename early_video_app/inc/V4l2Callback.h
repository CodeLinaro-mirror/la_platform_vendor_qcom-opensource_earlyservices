/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef __MSM_V4L2_CALLBACK_H__
#define __MSM_V4L2_CALLBACK_H__

#include <list>
#include <mutex>

#include "V4l2Decoder.h"

class V4l2Callback : public V4l2DriverCb {
    public:
        V4l2Callback(V4l2Decoder* dec);
        ~V4l2Callback();

        int onV4l2BufferDone(struct v4l2_buffer* buffer);
        void onV4l2EventDone(struct v4l2_event* event);
        int onV4l2Error(int error);
    private:
        V4l2Codec* mV4l2Codec;
};

#endif
