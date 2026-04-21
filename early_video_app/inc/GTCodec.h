/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef __GT_CODEC_H__
#define __GT_CODEC_H__

#include <sys/mman.h>
#include <list>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <chrono>
#include <unordered_map>

#ifndef ANDROID
#include <client_vidc_interface.h>
#include "../../common/inc/platform_ops.h"
#endif
#include "V4l2Codec.h"
#include "EventHandler.h"
#include "VideoDefines.h"
#include "VidcLog.h"

class GTCodec;

void ThreadFunc(GTCodec& codec);

class GTCodec {
    public:
        GTCodec(unsigned int codec, unsigned int colorFmt);
        virtual ~GTCodec();
        virtual int queueBuffers() = 0;
        virtual int gtRegisterCallbacks() = 0;

        int gtCodecRunSSR();
        int gtCodecRunStabilityCmd();
        int gtCodecInit();
        bool gtIsAPVSupported();
        int gtSetResolution(unsigned int width, unsigned int height);
        int gtSetStride(unsigned int stride);
        int gtSetScanline(unsigned int scanline);
        int gtCodecSetColorFmt();
        int gtSetOperatingRate(unsigned int numer, unsigned int denom);
        int gtSetFrameRate(unsigned int numer, unsigned int denom);
        int gtGetOperatingRate();
        float gtGetFrameRate();
        int gtCodecConfigInput();
        int gtCodecConfigOutput();
        struct v4l2_format* getOutputFormat();
        int gtCodecStartOutput();
        int gtCodecStartInput();
        int gtCodecStartMetaOutput();
        int gtCodecStartMetaInput();
        int allocateMetaBuffers(enum port_type port);
        int allocateBuffers(enum port_type port);
        int allocateBuffersSingleFd(enum port_type port);
        int gtCodecStopInput();
        int gtCodecStopOutput();
        int gtCodecStopMetaInput();
        int gtCodecStopMetaOutput();
        void freeBuffers(enum port_type port);
        void freeBuffersSingleFd(enum port_type port);
        void gtCodecDeInit();
        int getTotalFramesDone();
        int queueSingleBufferEnable(bool enable);
        int enableImmediateStopPostDrain(bool enable);
        void configGTControls(unsigned int id, int value);
        void configGTDynamicParams(unsigned int id, int value, unsigned int frameNum);
        int queryControl(struct v4l2_queryctrl *queryctrl);
        int queryMenu(struct v4l2_querymenu* querymenu);
        void setEarlyNotifyIntrptCount(uint32_t count);
        int setGTControls();
        /* set Operating rate */
        int setOperatingRate(const unsigned int& opRate);
        /* set Frame rate */
        int setFrameRate(const unsigned int& fps);
        /* set Multiplier */
        int setMultiplier(const unsigned int& cnt);
        /* set SkipReadAfterNFrames */
        int setSkipReadAfterNFrames(const unsigned int& cnt);
        /* get Multiplier */
        unsigned int getMultiplier();
        void gtCodecReset();
        void addFrameStats(uint32_t sizeBytes);
        float getAvgFrameRate();
        float getAvgBitRate();
        void setLastFlagEvent(int value);
        void handleStats();
        void threadLoop();
        int createStatsThread();
        int stopStatsThread();
        int gtSetColorSpace(enum port_type port, unsigned int colorPrimaries, unsigned int matrixCoeff,
            unsigned int transferChar, unsigned int range);
        int gtGetColorSpace(enum port_type port, unsigned int *colorPrimaries, unsigned int *matrixCoeff,
            unsigned int *transferChar, unsigned int *range);
        bool isUBWCFormat();

    public:
        bool mIFPC = false;
        int mFBDcount = 0;
        int mHWResetAtFrameNum = -1;
        int mSSRatFrameNum = -1;
        char *mStabilityValueString = nullptr;
        char *mSSRValueString = nullptr;

    protected:
        std::shared_ptr<V4l2Codec> mV4l2Codec;

        bool mDrainSent = false;
        bool mDrainPending = false;
        bool mReconfigEventReceived = false;
        bool mErrorReceived = false;
        bool mStatsThreadExit = false;
        bool mStatsThreadRunning = false;
        bool mDrainLastFlagReceived = false;
        bool mIsQueueSingleBufferEnabled = false;
        bool mImmediateStopPostDrainEnabled = false;
        bool mLastFlagEventEnabled = false;

        int mTotalFramesDone = 0;

        unsigned int mCodec;
        unsigned int mColorfmt;
        unsigned int mFrameRate = 30;
        unsigned int mMultiplier = 1;
        unsigned int mOperatingRate = 30;
        unsigned int mSkipReadAfterNFrames = 0;

        std::shared_ptr<std::thread> mStatsThread;

        // StreamProcessor Hooks
        std::unordered_map<std::string, std::string> gStreamProcessorCodecMap = {
            // Pixel Formats
            { "NV12",                   "Parse/NV12"                },
            { "NV21",                   "Parse/NV21"                },
            { "P010",                   "Parse/P010"                },
            { "P210",                   "Parse/P210"                },
            { "P210_UBWC",              "Parse/P210_UBWC"           },
            { "UBWC_10bit",             "Parse/TP10_UBWC"           },
            { "UBWC_NV12_8bit",         "Parse/NV12_UBWC"           },
            { "PLANAR_CRCB",            "Parse/YV12"                },
            { "RGBA8888",               "Parse/RGBA32"              },
            { "RGBA_FP16",              "Parse/RGBAFP16"            },
            { "RGBA1010102",            "Parse/RGBA1010102"         },

            //Codecs
            { "VIDEO_CodingAVC",        "Parse/AVC"                 },
            { "VIDEO_CodingAV1",        "Parse/AV1"                 },
            { "VIDEO_CodingVP9",        "Parse/VPX"                 },
            { "VIDEO_CodingHEVC",       "Parse/HEVC"                },
            { "VIDEO_CodingAPV",        "Parse/APV"                 },
        };

        std::string mStreamProcessorType;

        struct MovingWindow {
            static constexpr const float kDefaultFps = 1.2;
            explicit MovingWindow(uint32_t windowSize);
            void reset();
            void addFrame(uint32_t sizeBytes);
            float fps() const;
            float kbps() const;

            private:
                const uint32_t mWindowSize;
                int64_t mStartUs;
                uint64_t mFrameSizeTotal;
                struct FrameInfo {
                    int64_t mTime;
                    uint32_t mSize;
                    FrameInfo(int64_t time, uint32_t size) {
                        mTime = time;
                        mSize = size;
                    };
                };
                std::list<FrameInfo> mFrames;
        };
        MovingWindow mMWBitrate;
};

#endif
