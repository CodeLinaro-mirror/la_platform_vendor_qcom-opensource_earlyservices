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
#include <display/drm/sde_drm.h>
#include <drm/drm_fourcc.h>
#include <libdrm_macros.h>

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

    fb_obj fb;

    int x_offset;
    int x_increment;
    bool first_run = true;
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

struct drm_buffer_context {
    int dma_heap_fd = -1;
    uint32_t gem_handle;
    uint32_t fb_id;
    std::vector<fb_obj> fb_objs;
    std::vector<int> drm_fence_fds;
};

class DisplayAdaptor {
    public:
        DisplayAdaptor(void);
        bool isInitialized();
        bool initAdaptor(uint32_t imgWidth, uint32_t imgHeight,
            uint32_t dataFrameWidth, uint32_t dataFrameHeight, uint32_t dataFrameSize,
            const char* displayCard);
        bool commitData(std::uint8_t* pBuffer, uint32_t length, uint32_t offset);
        bool deinitAdaptor(void);

    private:
        int parseDisplay();
        int createFrameBuffer(uint32_t dataFrameWidth, uint32_t dataFrameHeight, uint32_t dataFrameSize);
        bool updateFrameBuffer(int connectorCfgIndex, int& drmOutFenceFD);
        int setupConnector();
        bool atomicCommit();
        void updatePossibleCrtcs(void);
        int getPropId(uint32_t objId, uint32_t objType, const char *name, uint32_t *propIdOut);
        void waitDRMOutFenceAndReset(int& fenceFD, int timeoutMS);

        void destroyBuff(int keepLatestItemCount);

    private:
        drmModeAtomicReqPtr mDRMModeReqPtr;
        std::vector<plane_config> mPlaneCfgVector;
        std::vector<connector_config> mConnectorCfgVector;
        std::vector<drm_buffer_context> mLastDRMBufferContextVector;

        char* mDisplayCard;
        int mDisplayCardFD;
        int mDMAHeapDeaviceFD;
        uint32_t mDRMOutFencePtrPropID;
        drm_buffer_context* mLastDrmBufferContext;
        int mSourceImgWidth;
        int mSourceImgHeight;
        int mSourceDataFrameWidth;
        int mSourceDataFrameHeight;
};

#endif