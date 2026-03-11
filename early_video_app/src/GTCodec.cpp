/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include "GTCodec.h"

using namespace early_video_app;

GTCodec::GTCodec(unsigned int codec, unsigned int colorFmt) :
        mCodec(codec),
        mColorfmt(colorFmt),
        /* 2 x max POR fps(960) */
        mMWBitrate(1920u) {
        VIDC_HIGH("GTCodec, constructor\n");
#if !defined(ANDROID) && !defined(_LINUX_VENV_)
        g_pltfrm_ops = platform_init();
        vidc_init();
#endif
}

GTCodec::~GTCodec() {
        VIDC_HIGH("GTCodec, destructor\n");
#if !defined(ANDROID) && !defined(_LINUX_VENV_)
        vidc_exit();
        platform_deinit(g_pltfrm_ops);
        /* clean the stream processor instance */
        mVspInstance.release();
#endif
}

int GTCodec::gtCodecRunSSR() {

    /* if ssr_value is null, return */
    if (mSSRValueString == nullptr)
        return 0;

    VIDC_HIGH("GTCodec::gtCodecRunSSR, mSSRValueString: %s\n", mSSRValueString);
    /*
    * below commands need to be executed to mount debugfs
    * adb shell setprop persist.dbg.keep_debugfs_mounted true
    * adb reboot
    * or
    * adb shell "mount -t debugfs none /sys/kernel/debug"
    */
    FILE *fp = fopen("/sys/kernel/debug/msm_vidc/core/trigger_ssr", "w");
    if (fp) {
        fwrite(mSSRValueString, sizeof(char), strlen(mSSRValueString), fp);
        fclose(fp);
    } else {
        VIDC_ERR("GTCodec::gtCodecRunSSR, error in opening trigger_ssr file. could be Missing mount fs.");
        return -1;
    }

    return 0;
}

int GTCodec::gtCodecRunStabilityCmd() {

    /* If StabilityValueString is null, return */
    if(mStabilityValueString == nullptr)
        return 0;

    VIDC_HIGH("GTCodec::gtCodecRunStabilityCmd, mStabilityValueString: %s\n", mStabilityValueString);
    /*
    * below commands need to be executed to mount debugfs
    * adb shell setprop persist.dbg.keep_debugfs_mounted true
    * adb reboot
    * or
    * adb shell "mount -t debugfs none /sys/kernel/debug"
    */
    FILE *fp = fopen("/sys/kernel/debug/msm_vidc/core/trigger_stability", "w");
    if (fp) {
        fwrite(mStabilityValueString, sizeof(char), strlen(mStabilityValueString), fp);
        fclose(fp);
    } else {
        VIDC_ERR("GTCodec::gtCodecRunStabilityCmd, error in opening trigger_stability file. could be Missing mount fs.");
        return -1;
    }
    return 0;
}

int GTCodec::gtCodecInit() {
    return mV4l2Codec->init(mCodec);
}

int GTCodec::gtSetResolution(unsigned int width, unsigned int height) {
    int ret = 0;

    ret = mV4l2Codec->setWidth(width);
    if (ret)
        return ret;

    ret = mV4l2Codec->setHeight(height);
    if (ret)
        return ret;

    return ret;
}

int GTCodec::gtSetStride(unsigned int stride) {
    return mV4l2Codec->setStride(stride);
}

int GTCodec::gtSetScanline(unsigned int scanline) {
    return mV4l2Codec->setScanline(scanline);
}

int GTCodec::gtCodecSetColorFmt() {
    return mV4l2Codec->setColorFormat(mColorfmt);
}

int GTCodec::gtSetOperatingRate(unsigned int numer, unsigned int denom) {
    return mV4l2Codec->setOperatingRate(numer, denom);
}

int GTCodec::gtSetFrameRate(unsigned int numer, unsigned int denom) {
    return mV4l2Codec->setFrameRate(numer, denom);
}

int GTCodec::gtGetOperatingRate() {
    return mV4l2Codec->getOperatingRate();
}

int GTCodec::gtGetFrameRate() {
    return mV4l2Codec->getFrameRate();
}

int GTCodec::gtCodecConfigInput() {
    return mV4l2Codec->configureInput();
}

int GTCodec::gtCodecConfigOutput() {
    return mV4l2Codec->configureOutput();
}

int GTCodec::gtCodecStartOutput() {
    return mV4l2Codec->startOutput();
}

int GTCodec::gtCodecStartInput() {
    return mV4l2Codec->startInput();
}

int GTCodec::gtCodecStartMetaOutput() {
    return mV4l2Codec->startMetaOutput();
}

int GTCodec::gtCodecStartMetaInput() {
    return mV4l2Codec->startMetaInput();
}

int GTCodec::allocateMetaBuffers(enum port_type port) {
    return mV4l2Codec->allocateMetaBuffers(port);
}

int GTCodec::allocateBuffers(enum port_type port) {
    return mV4l2Codec->allocateBuffers(port);
}

int GTCodec::allocateBuffersSingleFd(enum port_type port) {
    return mV4l2Codec->allocateBuffersSingleFd(port);
}

int GTCodec::gtCodecStopInput() {
    return mV4l2Codec->stopInput();
}

int GTCodec::gtCodecStopOutput() {
    return mV4l2Codec->stopOutput();
}

int GTCodec::gtCodecStopMetaInput() {
    return mV4l2Codec->stopMetaInput();
}

int GTCodec::gtCodecStopMetaOutput() {
    return mV4l2Codec->stopMetaOutput();
}

void GTCodec::freeBuffers(enum port_type port) {
    mV4l2Codec->freeBuffers(port);
}

void GTCodec::freeBuffersSingleFd(enum port_type port) {
    mV4l2Codec->freeBuffersSingleFd(port);
}

void GTCodec::gtCodecDeInit() {
    mV4l2Codec->deinit();
}

int GTCodec::getTotalFramesDone() {
    return mTotalFramesDone;
}

int GTCodec::queueSingleBufferEnable(bool enable) {
    mIsQueueSingleBufferEnabled = enable;
    return 0;
}

int GTCodec::enableImmediateStopPostDrain(bool enable) {
    mImmediateStopPostDrainEnabled = enable;
    return 0;
}

void GTCodec::configGTControls(unsigned int id, int value) {
    auto ctrlInfo = std::make_shared<V4L2ControlInfo>();
    ctrlInfo->id = id;
    ctrlInfo->value = value;

    if (id == V4L2_CID_MPEG_VIDC_LAST_FLAG_EVENT_ENABLE && value)
        setLastFlagEvent(value);

    if (id == V4L2_CID_MPEG_VIDEO_HEVC_PROFILE)
        mV4l2Codec->setProfile(value);

    mV4l2Codec->mControls.push_back(ctrlInfo);
}

void GTCodec::configGTDynamicParams(unsigned int id, int value, unsigned int frameNum) {
    auto ctrlInfo = std::make_shared<V4L2DynamicParams>();
    ctrlInfo->controlId = id;
    ctrlInfo->value = value;
    ctrlInfo->frameNum = frameNum;

    mV4l2Codec->mDynamicParams.push_back(ctrlInfo);
}

int GTCodec::queryControl(struct v4l2_queryctrl *queryctrl) {
    int ret = 0;

    if (!queryctrl) {
        VIDC_ERR("GTCodec::queryControl, Invalid param\n");
        return -EINVAL;
    }

    ret = mV4l2Codec->queryControl(queryctrl);
    if (ret) {
        VIDC_ERR("GTCodec::queryControl, queryControl failed\n");
        return ret;
    }

    return 0;
}

int GTCodec::queryMenu(struct v4l2_querymenu* querymenu) {
    int ret = 0;

    if (!querymenu) {
        VIDC_ERR("GTCodec::queryMenu, Invalid param\n");
        return -EINVAL;
    }

    ret = mV4l2Codec->queryMenu(querymenu);
    if (ret) {
        VIDC_ERR("GTCodec::queryMenu, queryMenu failed\n");
        return ret;
    }

    return 0;
}

void GTCodec::setEarlyNotifyIntrptCount(uint32_t count) {
    mV4l2Codec->setEarlyNotifyIntrptCount(count);
}

int GTCodec::setGTControls() {
    int ret = 0;

    ret = mV4l2Codec->setV4l2Controls();
    return ret;
}

/* set Operating rate */
int GTCodec::setOperatingRate(const unsigned int& opRate) {
    mOperatingRate = opRate;
    return 0;
}

/* set Frame rate */
int GTCodec::setFrameRate(const unsigned int& fps) {
    mFrameRate = fps;
    return 0;
}

/* set Multiplier */
int GTCodec::setMultiplier(const unsigned int& cnt) {
    mMultiplier = cnt;
    mV4l2Codec->setMultiplier(mMultiplier);
    return 0;
}

/* set SkipReadAfterNFrames */
int GTCodec::setSkipReadAfterNFrames(const unsigned int& cnt) {
    mSkipReadAfterNFrames = cnt;
    return 0;
}

/* get Multiplier */
unsigned int GTCodec::getMultiplier() {
    return mMultiplier;
}

void GTCodec::gtCodecReset() {
    mMWBitrate.reset();
}

void GTCodec::addFrameStats(uint32_t sizeBytes) {
    mMWBitrate.addFrame(sizeBytes);
}

float GTCodec::getAvgFrameRate() {
    return mMWBitrate.fps();
}

float GTCodec::getAvgBitRate() {
    return mMWBitrate.kbps();
}

void GTCodec::setLastFlagEvent(int value) {
    mLastFlagEventEnabled = value;
}

void GTCodec::handleStats() {
    auto avgBitrate = getAvgBitRate();
    bool mbps = avgBitrate > 1024.00;

    VIDC_MED("GTCodec::handleStats, frameRate %.2f fps, bitRate %.1f %s\n",
        getAvgFrameRate(),
        mbps ? avgBitrate / 1024 : avgBitrate,
        mbps ? "Mbps" : "Kbps");
}

void GTCodec::threadLoop() {
    mStatsThreadRunning = true;
    while (!mStatsThreadExit)
    {
        handleStats();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    VIDC_MED("GTCodec::threadLoop, end\n");
}

int GTCodec::createStatsThread() {
    mStatsThreadExit = false;
    mStatsThreadRunning = false;
    mStatsThread = std::make_shared<std::thread>(ThreadFunc, std::ref(*this));
    if (!mStatsThread) {
        VIDC_ERR("GTCodec::createStatsThread, thread create failed\n");
        return -EINVAL;
    }
    else {
        int count = 0;
        while (!mStatsThreadRunning) {
            VIDC_MED("GTCodec::createStatsThread, wait for thread running\n");
            usleep(5 * 1000);
            count++;
            if (count >= 100)
                break;
        }
        if (!mStatsThreadRunning) {
            VIDC_ERR("GTCodec::createStatsThread, thread not running\n");
            return -EINVAL;
        }
    }
    VIDC_MED("GTCodec::createStatsThread, thread started\n");
    return 0;
}

int GTCodec::stopStatsThread() {
    if (!mStatsThread) {
        VIDC_ERR("GTCodec::stopStatsThread, invalid stats thread\n");
        return -EINVAL;
    }

    mStatsThreadExit = true;
    VIDC_MED("GTCodec::stopStatsThread, join thread\n");
    if (mStatsThread != nullptr and mStatsThread->joinable()) {
        mStatsThread->join();
    }
    VIDC_MED("GTCodec::stopStatsThread, exit stats thread\n");
    return 0;
}

int GTCodec::gtSetColorSpace(enum port_type port, unsigned int colorPrimaries, unsigned int matrixCoeff,
    unsigned int transferChar, unsigned int range) {
    return mV4l2Codec->setColorSpaceInfo(port, colorPrimaries, matrixCoeff, transferChar, range);
}

int GTCodec::gtGetColorSpace(enum port_type port, unsigned int *colorPrimaries, unsigned int *matrixCoeff,
    unsigned int *transferChar, unsigned int *range) {
    return mV4l2Codec->getColorSpaceInfo(port, colorPrimaries, matrixCoeff, transferChar, range);
}

bool GTCodec::isUBWCFormat() {
    if (mColorfmt == V4L2_PIX_FMT_VIDC_NV12C ||
        mColorfmt == V4L2_PIX_FMT_VIDC_TP10C ||
        mColorfmt == V4L2_PIX_FMT_VIDC_ARGB32C)
        return true;

    return false;
}

GTCodec::MovingWindow::MovingWindow(uint32_t windowSize)
    : mWindowSize(windowSize),
        mFrameSizeTotal(0ull),
        mStartUs(std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count()) {
}

void GTCodec::MovingWindow::reset() {
    mFrameSizeTotal = 0ull;
    mFrames.clear();
}

void GTCodec::MovingWindow::addFrame(uint32_t sizeBytes) {
    uint64_t timeUs = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    mFrames.emplace_back(timeUs - mStartUs, sizeBytes);
    mFrameSizeTotal += sizeBytes;
    if (mFrames.size() > mWindowSize) {
        mFrameSizeTotal -= mFrames.front().mSize;
        mFrames.pop_front();
    }
}

float GTCodec::MovingWindow::fps() const {
    if (mFrames.empty()) {
        return kDefaultFps;
    }
    uint64_t dt_us = mFrames.back().mTime - mFrames.front().mTime;
    uint32_t counter = mFrames.size();
    float fps = dt_us == 0 ? kDefaultFps : (1e6 * counter) / static_cast<float>(dt_us);
    return fps;
}

float GTCodec::MovingWindow::kbps() const {
    auto dt = mFrames.back().mTime - mFrames.front().mTime;
    float kbps = dt == 0 ? 0.0 : (1e6 * 8 * mFrameSizeTotal) / (dt * 1024);
    return kbps;
}