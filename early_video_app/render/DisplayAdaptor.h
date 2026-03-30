/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef __DISPLAY_ADAPTOR_H__
#define __DISPLAY_ADAPTOR_H__

#include <vector>
#include <cstdint>

#include <drm/msm_drm.h>

#include "xf86drm.h"
#include "xf86drmMode.h"
#include "../../../display-drivers/include/uapi/display/drm/sde_drm.h"
#include "../../../display/libdrm/include/drm/drm_fourcc.h"
#include "../../../display/libdrm/libdrm_macros.h"

#define MAX_BUFFER                      1

struct fb_obj {
    uint32_t fb_id;
    size_t size;
    size_t pitch;
    size_t offset;
    unsigned handle;
    void *ptr;
    void *plane[2];
    int ion_fd;
    int ion_map_fd;
};

struct connector_config {
    uint32_t connector_id;
    uint32_t crtc_id;
    uint32_t crtc_idx;
    uint32_t width;
    uint32_t height;

    uint32_t mode_id;
    drmModeModeInfo mode;

    uint32_t crtc_id_pid;
    uint32_t active_pid;
    uint32_t mode_id_pid;
};

struct plane_config {
    uint32_t plane_id;
    uint32_t crtc_id;
    uint32_t connector_id;

    fb_obj fb[MAX_BUFFER];

    int x_offset;
    int x_increment;
    bool first_run;
    bool handoff_init;
    uint32_t possible_crtcs;

    int x;
    int y;
    int w;
    int h;
    int zpos;
    int format;

    /* optional */
    int handoff;
    bool handoff_set;

    /* properties */
    uint32_t crtc_pid;
    uint32_t fb_pid;
    uint32_t zpos_pid;
    uint32_t crtc_x_pid;
    uint32_t crtc_y_pid;
    uint32_t crtc_w_pid;
    uint32_t crtc_h_pid;
    uint32_t src_x_pid;
    uint32_t src_y_pid;
    uint32_t src_w_pid;
    uint32_t src_h_pid;
    uint32_t handoff_pid;
};

class DisplayAdaptor {
    public:
        DisplayAdaptor(void);
        bool isInitialized();
        bool initAdaptor(uint32_t frameWidth, uint32_t frameHeight, uint32_t dataSize);
        bool commitData(std::uint8_t* pBuffer, uint32_t length, uint32_t offset);
        bool deinitAdaptor(void);

    private:
        int parseDisplay();
        int createFrameBufferForNV12(uint32_t frameWidth, uint32_t frameHeight);
        int createFrameBufferForNV12UBWC(uint32_t frameWidth, uint32_t frameHeight, uint32_t dataSize);
        bool updateFrameBuffer(int connectorCfgIndex, int bufIndex);
        int setupConnector();
        bool atomicCommit(bool isAsync);
        void updatePossibleCrtcs(void);
        int getPropId(uint32_t objId, uint32_t objType, const char *name, uint32_t *propIdOut);

        int destroyBuff();

    private:
        drmModeAtomicReqPtr mDRMModeReqPtr;
        std::vector<plane_config> mPlaneCfgVector;
        std::vector<connector_config> mConnectorCfgVector;

        int mDisplayCardFD;
        int mDMAHeapDeaviceFD;
        int mSourceFrameWidth;
        int mSourceFrameHeight;
        int mCurrentFrameBufferIndex;
};

#endif