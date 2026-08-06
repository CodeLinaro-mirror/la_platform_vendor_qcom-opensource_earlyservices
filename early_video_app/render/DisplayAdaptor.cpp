/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include <cstdint>
#include <mutex>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <atomic>
#include <stdint.h>
#include <iostream>
#include <cstring>

#include <linux/ioctl.h>
#include <stdint.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cutils/properties.h>

#include "DisplayAdaptor.h"
#include "../inc/VidcLog.h"

#define FIXED_16_16(x)        ((uint64_t)(x) << 16)

struct dma_heap_allocation_data {
    __u64 len;        // total size in bytes
    __u32 fd;         // returned dma-buf fd
    __u32 fd_flags;   // e.g., O_RDWR | O_CLOEXEC
    __u64 heap_flags; // always 0 for system heap
};

// DMA-HEAP magic
#define DMA_HEAP_IOC_MAGIC    'H'
// alloc ioctl
#define DMA_HEAP_IOCTL_ALLOC  _IOWR(DMA_HEAP_IOC_MAGIC, 0x0, struct dma_heap_allocation_data)

const char* DMADevicePath = "/dev/dma_heap/qcom,system";
static const char* ConnectorTypeNames[] = {
   "unknown",
   "VGA",
   "DVI-I",
   "DVI-D",
   "DVI-A",
   "composite",
   "s-video",
   "LVDS",
   "component",
   "9-pin DIN",
   "DP",
   "HDMI-A",
   "HDMI-B",
   "TV",
   "eDP",
   "Virtual",
   "DSI",
};
const uint64_t PixelFormat = DRM_FORMAT_NV12;
//0(when not DRM_FORMAT_MOD_QCOM_COMPRESSED), DRM_FORMAT_MOD_QCOM_COMPRESSED
const uint64_t FrameBuferModifier = DRM_FORMAT_MOD_QCOM_COMPRESSED;

const int DRMOutFencePollTimeOutMS = 500;

std::mutex initMutex;
std::mutex planeMutex;
std::atomic<bool> isAdaptorInitialized{false};

using namespace early_video_app;

DisplayAdaptor::DisplayAdaptor() :
   mDisplayCardFD(-1),
   mDMAHeapDeaviceFD(-1),
   mDRMModeReqPtr(NULL),
   mLastDrmBufferContext(NULL) {

}

bool DisplayAdaptor::isInitialized() {
   return isAdaptorInitialized.load();
}

bool DisplayAdaptor::initAdaptor(uint32_t imgWidth, uint32_t imgHeight,
            uint32_t dataFrameWidth, uint32_t dataFrameHeight, uint32_t dataFrameSize,
            const char* displayCard) {
   VIDC_HIGH("DisplayAdaptor::initAdaptor, imgWidth = %d, imgHeight = %d\n", imgWidth, imgHeight, dataFrameWidth);
   VIDC_HIGH("DisplayAdaptor::initAdaptor, dataFrameWidth = %d, dataFrameHeight = %d, dataFrameSize = %d\n",
         dataFrameWidth, dataFrameHeight, dataFrameSize);
   VIDC_HIGH("DisplayAdaptor::initAdaptor, displayCard = %s, FrameBuferModifier = %d\n",
      displayCard == NULL ? "NULL" : displayCard, FrameBuferModifier);
   if (imgWidth <= 0 || imgHeight <= 0 ||
      dataFrameWidth <= 0 || dataFrameHeight <= 0 || dataFrameSize <= 0 ||
      displayCard == NULL) {
      VIDC_ERR("DisplayAdaptor::initAdaptor, params illegal\n");
      return false;
   }
   initMutex.lock();
   if (isAdaptorInitialized.load()) {
      bool result = (imgWidth == mSourceImgWidth && imgHeight == mSourceImgHeight &&
         strcmp(mDisplayCard, displayCard) == 0);
      initMutex.unlock();
      return result;
   }
   VIDC_HIGH("DisplayAdaptor::initAdaptor, open display card: %s\n", displayCard);
   mDisplayCardFD = open(displayCard, O_RDWR, 0);
   if (mDisplayCardFD < 0) {
      VIDC_ERR("DisplayAdaptor::initAdaptor, failed to open %s\n", displayCard);
      initMutex.unlock();
      return false;
   }
   mDisplayCard = (char*)displayCard;
   drmSetClientCap(mDisplayCardFD, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
   drmSetClientCap(mDisplayCardFD, DRM_CLIENT_CAP_ATOMIC, 1);

   VIDC_HIGH("DisplayAdaptor::initAdaptor, parse display\n");
   int result = parseDisplay();
   if (result != 0) {
      VIDC_ERR("DisplayAdaptor::initAdaptor, parse_display failed\n");
      initMutex.unlock();
      return false;
   }

   VIDC_HIGH("DisplayAdaptor::initAdaptor, create frame buffer\n");
   if (PixelFormat == DRM_FORMAT_NV12) {
      if (FrameBuferModifier == 0) {
         int result = createFrameBuffer(dataFrameWidth, dataFrameHeight, dataFrameSize);
         if (result != 0) {
            VIDC_ERR("DisplayAdaptor::initAdaptor, Failed to create framebuffer\n");
            initMutex.unlock();
            return false;
         }
      }
      else if (FrameBuferModifier == DRM_FORMAT_MOD_QCOM_COMPRESSED) {
         //will create frame buffer at commitData.
      }
      else {
         VIDC_ERR("DisplayAdaptor::initAdaptor, not supported format 0x%x except NV12 and NV12UBWC\n", PixelFormat);
         initMutex.unlock();
         return false;
      }
   }
   else {
      VIDC_ERR("DisplayAdaptor::initAdaptor, not supported format 0x%x except NV12\n", PixelFormat);
      initMutex.unlock();
      return false;
   }

   /* init atomic commit */
   VIDC_HIGH("DisplayAdaptor::initAdaptor, alloc atomic commit\n");
   mDRMModeReqPtr = drmModeAtomicAlloc();
   if (mDRMModeReqPtr == NULL) {
      VIDC_ERR("DisplayAdaptor::initAdaptor, failed to init atomic commit\n");
      result = false;
      initMutex.unlock();
      return result;
   }

   /* setup crtc / connector */
   VIDC_HIGH("DisplayAdaptor::initAdaptor, setup connector\n");
   result = setupConnector();
   if (result != 0) {
      VIDC_ERR("DisplayAdaptor::initAdaptor, failed to setup connector\n");
      initMutex.unlock();
      return false;
   }
   VIDC_HIGH("DisplayAdaptor::initAdaptor, setup connector successfully\n");
   isAdaptorInitialized.store(true);
   mSourceImgWidth = imgWidth;
   mSourceImgHeight = imgHeight;
   mSourceDataFrameWidth = dataFrameWidth;
   mSourceDataFrameHeight = dataFrameHeight;
   initMutex.unlock();

   return true;
}

bool DisplayAdaptor::commitData(std::uint8_t* pBuffer, uint32_t length, uint32_t offset) {
   VIDC_MED("DisplayAdaptor::commitData, enter, length = %d, offset = %d\n", length, offset);
   if(length <= 0) {
      return false;
   }
   if (!isAdaptorInitialized.load()) {
      VIDC_MED("DisplayAdaptor::commitData, adaptor not initialized\n");
      return false;
   }

   //wait last frame buffer fence complete.
   if (mLastDrmBufferContext != NULL) {
      for (int i = 0; i < (int)mLastDrmBufferContext->drm_fence_fds.size(); i++) {
         waitDRMOutFenceAndReset(mLastDrmBufferContext->drm_fence_fds[i], DRMOutFencePollTimeOutMS);
      }
   }

   //create frame buffer when format NV12 UBWC.
   if (PixelFormat == DRM_FORMAT_NV12) {
      if (FrameBuferModifier == DRM_FORMAT_MOD_QCOM_COMPRESSED) {
         destroyBuff(2);
         VIDC_MED("DisplayAdaptor::commitData, create frame buffer\n");
         int result = createFrameBuffer(mSourceDataFrameWidth, mSourceDataFrameHeight, length);
         if (result != 0) {
            VIDC_ERR("DisplayAdaptor::commitData, Failed to create framebuffer\n");
            return false;
         }
      }
      else if (FrameBuferModifier == 0) {
         //frame buffer created at initAdaptor.
         //clear released drm fence fd
         if (mLastDrmBufferContext != NULL && mLastDrmBufferContext->drm_fence_fds.size() > mConnectorCfgVector.size()) {
            mLastDrmBufferContext->drm_fence_fds.erase(mLastDrmBufferContext->drm_fence_fds.begin(),
               mLastDrmBufferContext->drm_fence_fds.end() - mConnectorCfgVector.size());
         }
      }
      else {
         VIDC_ERR("DisplayAdaptor::commitData, not supported format 0x%x except NV12 and NV12UBWC\n", PixelFormat);
         return false;
      }
   }
   else {
      VIDC_ERR("DisplayAdaptor::commitData, not supported format 0x%x except NV12\n", PixelFormat);
      return false;
   }

   VIDC_MED("DisplayAdaptor::commitData, =======update fb and commit fb========\n");
   for (int i = 0; i < (int)mPlaneCfgVector.size(); i++) {
      if (mPlaneCfgVector[i].fb.ptr != NULL) {
         memcpy(mPlaneCfgVector[i].fb.ptr, pBuffer + offset, length);
      }
      /* update fb */
      int drmOutFenceFD = -1;
      bool result = updateFrameBuffer(i, drmOutFenceFD);
      if (!result) {
         VIDC_ERR("DisplayAdaptor::commitData, connector: %d commit failed\n", i);
         updatePossibleCrtcs();
      }
      else {
         static bool isFirstKPILogPrinted = false;
         if (!isFirstKPILogPrinted) {
            isFirstKPILogPrinted = true;
            printKPILog("%s%s%s", LogKPITag, LogAPPTag, "1st video frame rendered\n");
         }
	      VIDC_HIGH("GTDecoderIOAdapter::commitData, video frame rendered to screen\n");
      }
      VIDC_MED("DisplayAdaptor::commitData, drmOutFenceFD = %d, mLastDrmBufferContext->fb_id = %d\n",
         drmOutFenceFD, (mLastDrmBufferContext != NULL ? mLastDrmBufferContext->fb_id : -1));
      if (mLastDrmBufferContext != NULL) {
         mLastDrmBufferContext->drm_fence_fds.push_back(drmOutFenceFD);
      }
      VIDC_MED("DisplayAdaptor::commitData, succeeded\n");
   }
   return true;
}

bool DisplayAdaptor::deinitAdaptor() {
   if (mDRMModeReqPtr != NULL) {
      drmModeAtomicFree(mDRMModeReqPtr);
      mDRMModeReqPtr = NULL;
   }

   /* destroy frame buffer */
   destroyBuff(0);

   /* close fd */
   if (mDisplayCardFD >= 0) {
      close(mDisplayCardFD);
      mDisplayCardFD = -1;
   }
   if (mDMAHeapDeaviceFD >= 0) {
      close(mDMAHeapDeaviceFD);
      mDMAHeapDeaviceFD = -1;
   }
   isAdaptorInitialized.store(false);
   mSourceImgWidth = 0;
   mSourceImgHeight = 0;
   mSourceDataFrameWidth = 0;
   mSourceDataFrameHeight = 0;
   return 0;
}

int DisplayAdaptor::parseDisplay(void) {
   int connectorIndex = 0;
   int crtcIndex = 0;
   int planeIndex = 0;
   std::vector<connector_config> connectorCfgArray;
   drmModePlaneRes* planeRes = NULL;
   drmModeRes* displayRes = NULL;

   /* Get information about connectors */
   VIDC_MED("DisplayAdaptor::parseDisplay, get information about connectors\n");
   displayRes = drmModeGetResources(mDisplayCardFD);
   if (displayRes == NULL) {
      VIDC_ERR("DisplayAdaptor::parseDisplay, displayRes is NULL\n");
      goto out;
   }

   for (int i = 0; i < displayRes->count_connectors; i++) {
      uint32_t crtc_mask = 0;
      drmModeConnectorPtr connectorPtr = drmModeGetConnector(mDisplayCardFD, displayRes->connectors[i]);

      if (connectorPtr == NULL || connectorPtr->encoders == NULL) {
         VIDC_ERR("DisplayAdaptor::parseDisplay, connector: %d ptr is NULL or connectorPtr->encoders is NULL\n", i);
         goto out;
      }
      if (connectorPtr->connection != DRM_MODE_CONNECTED) {
         drmModeFreeConnector(connectorPtr);
         VIDC_MED("DisplayAdaptor::parseDisplay, connector: %d not connected\n\n", i);
         continue;
      }

      /* dump */
      VIDC_MED("DisplayAdaptor::parseDisplay, connectorIndex %d: %d - %s-%d crtcs:\n",
         connectorIndex, connectorPtr->connector_id,
         ConnectorTypeNames[connectorPtr->connector_type],
         connectorPtr->connector_type_id);
      for (int j = 0; j < connectorPtr->count_encoders; j++) {
         drmModeEncoderPtr encoderPtr = drmModeGetEncoder(mDisplayCardFD, connectorPtr->encoders[j]);
         if (encoderPtr == NULL) {
            VIDC_ERR("DisplayAdaptor::parseDisplay, encoderPtr is NULL\n");
            goto out;
         }

         crtc_mask |= encoderPtr->possible_crtcs;
         drmModeFreeEncoder(encoderPtr);
      }
      for (int j = 0; j < displayRes->count_crtcs; j++) {
         if (crtc_mask & (1 << j)) {
            VIDC_MED("DisplayAdaptor::parseDisplay, crtcs[%d] = %d\n", j, displayRes->crtcs[j]);
         }
      }
      for (int j = 0; j < connectorPtr->count_modes; j++) {
         VIDC_MED("DisplayAdaptor::parseDisplay, connectors[%d]->modes[%d]%s\n", i, j, connectorPtr->modes[j].name);
      }
      connector_config connectorConfig;
      connectorCfgArray.push_back(connectorConfig);
      connectorCfgArray[connectorIndex].connector_id = connectorPtr->connector_id;

      /* store property id */
      VIDC_MED("DisplayAdaptor::parseDisplay, store property id\n");
      drmModeObjectPropertiesPtr propertiesPtr = drmModeObjectGetProperties(mDisplayCardFD,
         connectorPtr->connector_id, DRM_MODE_OBJECT_CONNECTOR);

      if (propertiesPtr == NULL) {
         drmModeFreeConnector(connectorPtr);
         VIDC_ERR("DisplayAdaptor::parseDisplay, propertiesPtr is NULL\n");
         goto out;
      }

      for (int j = 0; j < (int)propertiesPtr->count_props; j++) {
         drmModePropertyPtr propertyPtr = drmModeGetProperty(mDisplayCardFD, propertiesPtr->props[j]);
         if (propertyPtr == NULL) {
            VIDC_ERR("DisplayAdaptor::parseDisplay, propertyPtr is NULL\n");
            drmModeFreeConnector(connectorPtr);
            drmModeFreeObjectProperties(propertiesPtr);
            goto out;
         }

         if (!strcmp(propertyPtr->name, "CRTC_ID")) {
            connectorCfgArray[connectorIndex].crtc_id_pid = propertyPtr->prop_id;
         }
         drmModeFreeProperty(propertyPtr);
      }

      /* copy mode */
      VIDC_MED("DisplayAdaptor::parseDisplay, copy mode\n");
      if (connectorPtr->modes == NULL) {
         drmModeFreeConnector(connectorPtr);
         drmModeFreeObjectProperties(propertiesPtr);
         VIDC_ERR("DisplayAdaptor::parseDisplay, connectorPtr->modes is NULL\n");
         goto out;
      }

      memcpy(&connectorCfgArray[connectorIndex].mode, &connectorPtr->modes[0], sizeof(drmModeModeInfo));
      drmModeFreeConnector(connectorPtr);
      drmModeFreeObjectProperties(propertiesPtr);
      connectorIndex++;
   }

   /* Get information about crtcs */
   VIDC_MED("DisplayAdaptor::parseDisplay, get information about crtcs\n");
   for (int i = 0; i < displayRes->count_crtcs; i++) {
      drmModeCrtcPtr crtcPtr = drmModeGetCrtc(mDisplayCardFD, displayRes->crtcs[i]);
      if (crtcPtr == NULL || displayRes->connectors == NULL) {
         VIDC_ERR("DisplayAdaptor::parseDisplay, crtc %d ptr is NULL or displayRes->connectors is NULL\n", i);
         goto out;
      }

      drmModeConnectorPtr connectorPtr = drmModeGetConnector(mDisplayCardFD, displayRes->connectors[i]);
      if (connectorPtr == NULL) {
         drmModeFreeCrtc(crtcPtr);
         VIDC_ERR("DisplayAdaptor::parseDisplay, connector %d ptr is NULL\n", i);
         goto out;
      }
      if (connectorPtr->connection != DRM_MODE_CONNECTED) {
         drmModeFreeCrtc(crtcPtr);
         drmModeFreeConnector(connectorPtr);
         VIDC_ERR("DisplayAdaptor::parseDisplay, connector %d ptr is not connected\n", i);
         continue;
      }

      connectorCfgArray[crtcIndex].crtc_id = crtcPtr->crtc_id;

      /* store property id */
      VIDC_MED("DisplayAdaptor::parseDisplay, store property id\n");
      drmModeObjectPropertiesPtr propertiesPtr = drmModeObjectGetProperties(mDisplayCardFD,
         crtcPtr->crtc_id, DRM_MODE_OBJECT_CRTC);

      if (propertiesPtr == NULL) {
         drmModeFreeCrtc(crtcPtr);
         drmModeFreeConnector(connectorPtr);
         VIDC_ERR("DisplayAdaptor::parseDisplay, crtc %d propertiesPtr is NULL\n", i);
         goto out;
      }

      for (int j = 0; j < (int)propertiesPtr->count_props; j++) {
         drmModePropertyPtr propertyPtr = drmModeGetProperty(mDisplayCardFD, propertiesPtr->props[j]);
         if (propertyPtr == NULL) {
            drmModeFreeCrtc(crtcPtr);
            drmModeFreeConnector(connectorPtr);
            drmModeFreeObjectProperties(propertiesPtr);
            VIDC_ERR("DisplayAdaptor::parseDisplay, propter %d ptr is NULL\n", i);
            goto out;
         }

         if (!strcmp(propertyPtr->name, "ACTIVE")) {
            connectorCfgArray[crtcIndex].active_pid = propertyPtr->prop_id;
         }
         else if (!strcmp(propertyPtr->name, "MODE_ID")) {
            connectorCfgArray[crtcIndex].mode_id_pid = propertyPtr->prop_id;
         }
         drmModeFreeProperty(propertyPtr);
      }

      /* store crtc index */
      VIDC_MED("DisplayAdaptor::parseDisplay, store crtc index: %d\n", crtcIndex);
      connectorCfgArray[crtcIndex].crtc_idx = crtcIndex;
      mConnectorCfgVector.push_back(connectorCfgArray[crtcIndex]);

      drmModeFreeCrtc(crtcPtr);
      drmModeFreeConnector(connectorPtr);
      drmModeFreeObjectProperties(propertiesPtr);
      crtcIndex++;
   }
   VIDC_MED("DisplayAdaptor::parseDisplay, mConnectorCfgVector.size = %d\n", mConnectorCfgVector.size());

   /*Get information about planes*/
   VIDC_MED("DisplayAdaptor::parseDisplay, get information about planes\n");
   planeRes = drmModeGetPlaneResources(mDisplayCardFD);
   if (planeRes == NULL) {
      VIDC_ERR("DisplayAdaptor::parseDisplay, planeRes is NULL\n");
      goto out;
   }

   for (int i = 0; i < (int)planeRes->count_planes; i++) {
      int handoff;
      drmModePlanePtr planePtr = drmModeGetPlane(mDisplayCardFD, planeRes->planes[i]);
      if (planePtr == NULL || displayRes->connectors == NULL) {
         VIDC_ERR("DisplayAdaptor::parseDisplay, plane %d ptr is NULL or displayRes->connectors is NULL\n", i);
         goto out;
      }

      drmModeConnectorPtr connectorPtr = drmModeGetConnector(mDisplayCardFD, displayRes->connectors[i]);
      if (connectorPtr == NULL) {
         drmModeFreePlane(planePtr);
         VIDC_ERR("DisplayAdaptor::parseDisplay, displayRes connector %d ptr is NULL\n", i);
         goto out;
      }
      if (connectorPtr->connection != DRM_MODE_CONNECTED) {
         drmModeFreePlane(planePtr);
         drmModeFreeConnector(connectorPtr);
         VIDC_ERR("DisplayAdaptor::parseDisplay, displayRes connector %d not connected\n", i);
         continue;
      }

      /* dump */
      VIDC_MED("DisplayAdaptor::parseDisplay, plane %d: %d - colorFormats:\n", i, planePtr->plane_id);
      for (uint32_t j = 0; j < planePtr->count_formats; j++) {
         uint32_t colorFormat = planePtr->formats[j];
         char* colorFormatName = drmGetFormatName(colorFormat);
         VIDC_MED("DisplayAdaptor::parseDisplay, colorFormat: %s, %d\n", colorFormatName, colorFormat);
      }

      VIDC_MED("DisplayAdaptor::parseDisplay, plane %d: %d - crtcs:\n", i, planePtr->plane_id);
      for (int j = 0; j < displayRes->count_crtcs; j++) {
         if (planePtr->possible_crtcs & (1 << j) && displayRes->crtcs) {
            VIDC_MED("DisplayAdaptor::parseDisplay, crtc: %d\n", displayRes->crtcs[j]);
         }
         else {
            VIDC_MED("DisplayAdaptor::parseDisplay, planePtr->possible_crtcs: %d, crtc[%d]: %d\n",
               planePtr->possible_crtcs, j, displayRes->crtcs[j]);
         }
      }
      plane_config planeCfg = {};
      planeCfg.plane_id = planePtr->plane_id;
      planeCfg.format = PixelFormat;

      /* store property id */
      VIDC_MED("DisplayAdaptor::parseDisplay, store property id\n");
      drmModeObjectPropertiesPtr propertiesPtr = drmModeObjectGetProperties(mDisplayCardFD,
         planePtr->plane_id, DRM_MODE_OBJECT_PLANE);
      if (propertiesPtr == NULL) {
         drmModeFreePlane(planePtr);
         drmModeFreeConnector(connectorPtr);
         VIDC_ERR("DisplayAdaptor::parseDisplay, propertiesPtr is NULL\n");
         goto out;
      }

      for (int j = 0; j < (int)propertiesPtr->count_props; j++) {
         drmModePropertyPtr propertyPtr = drmModeGetProperty(mDisplayCardFD, propertiesPtr->props[j]);
         if (propertyPtr == NULL) {
            drmModeFreePlane(planePtr);
            drmModeFreeConnector(connectorPtr);
            drmModeFreeObjectProperties(propertiesPtr);
            VIDC_ERR("DisplayAdaptor::parseDisplay, property %d propertiesPtr is NULL\n", j);
            goto out;
         }

         if (!strcmp(propertyPtr->name, "FB_ID")) {
            planeCfg.fb_pid = propertyPtr->prop_id;
         }
         else if (!strcmp(propertyPtr->name, "CRTC_ID")) {
            planeCfg.crtc_pid = propertyPtr->prop_id;
         }
         else if (!strcmp(propertyPtr->name, "zpos")) {
            planeCfg.zpos_pid = propertyPtr->prop_id;
         }
         else if (!strcmp(propertyPtr->name, "CRTC_X")) {
            planeCfg.crtc_x_pid = propertyPtr->prop_id;
         }
         else if (!strcmp(propertyPtr->name, "CRTC_Y")) {
            planeCfg.crtc_y_pid = propertyPtr->prop_id;
         }
         else if (!strcmp(propertyPtr->name, "CRTC_W")) {
            planeCfg.crtc_w_pid = propertyPtr->prop_id;
         }
         else if (!strcmp(propertyPtr->name, "CRTC_H")) {
            planeCfg.crtc_h_pid = propertyPtr->prop_id;
         }
         else if (!strcmp(propertyPtr->name, "SRC_X")) {
            planeCfg.src_x_pid = propertyPtr->prop_id;
         }
         else if (!strcmp(propertyPtr->name, "SRC_Y")) {
            planeCfg.src_y_pid = propertyPtr->prop_id;
         }
         else if (!strcmp(propertyPtr->name, "SRC_W")) {
            planeCfg.src_w_pid = propertyPtr->prop_id;
         }
         else if (!strcmp(propertyPtr->name, "SRC_H")) {
            planeCfg.src_h_pid = propertyPtr->prop_id;
         }
         else if (!strcmp(propertyPtr->name, "handoff")) {
            planeCfg.handoff_pid = propertyPtr->prop_id;
            handoff = propertiesPtr->prop_values[j];
         }
         drmModeFreeProperty(propertyPtr);
      }

      /* update possible_crtcs */
      VIDC_MED("DisplayAdaptor::parseDisplay, update possible_crtcs\n");
      planeCfg.possible_crtcs = planePtr->possible_crtcs;
      if (planePtr->possible_crtcs & (1 << i) && displayRes->crtcs) {
         planeCfg.crtc_id = displayRes->crtcs[i];
      }
      else {
         VIDC_MED("DisplayAdaptor::parseDisplay, planePtr->possible_crtcs: %d, displayRes->crtcs[%d]: %d\n",
            planePtr->possible_crtcs, i, displayRes->crtcs[i]);
      }

      /* dump handoff status */
      VIDC_MED("DisplayAdaptor::parseDisplay, dump handoff status\n");
      if (planeCfg.handoff_pid) {
         VIDC_MED("DisplayAdaptor::parseDisplay, handoff = %d possible_crtcs = 0x%x\n", handoff, planePtr->possible_crtcs);
      }
      planeCfg.x = planePtr->x;
      planeCfg.y = planePtr->y;
      planeCfg.h = mConnectorCfgVector[planeIndex].mode.vdisplay;
      planeCfg.w = mConnectorCfgVector[planeIndex].mode.hdisplay;
      VIDC_MED("DisplayAdaptor::parseDisplay, planeIndex = %d, planeCfg.x = %d, planeCfg.y = %d, planeCfg.w = %d, planeCfg.h = %d\n",
         planeIndex, planeCfg.x, planeCfg.y, planeCfg.w, planeCfg.h);

      mPlaneCfgVector.push_back(planeCfg);

      drmModeFreePlane(planePtr);
      drmModeFreeConnector(connectorPtr);
      drmModeFreeObjectProperties(propertiesPtr);
      planeIndex++;
   }
   VIDC_MED("DisplayAdaptor::parseDisplay, mPlaneCfgVector.size = %d\n", mPlaneCfgVector.size());
   drmModeFreePlaneResources(planeRes);
   drmModeFreeResources(displayRes);
   VIDC_MED("DisplayAdaptor::parseDisplay, parse finished\n");
   return 0;

out:
   if (displayRes != NULL) {
      drmModeFreeResources(displayRes);
   }
   if (planeRes != NULL) {
      drmModeFreePlaneResources(planeRes);
   }
   VIDC_MED("DisplayAdaptor::parseDisplay, break out\n");
   return -1;
}

int DisplayAdaptor::createFrameBuffer(uint32_t dataFrameWidth, uint32_t dataFrameHeight, uint32_t dataFrameSize) {
   VIDC_MED("DisplayAdaptor::createFrameBuffer, dataFrameWidth = %d, dataFrameHeight = %d, dataFrameSize = %d\n",
      dataFrameWidth, dataFrameHeight, dataFrameSize);
   int result = 0;
   /* create framebuffer */
   if (mDMAHeapDeaviceFD < 0) {
      mDMAHeapDeaviceFD = open(DMADevicePath, O_RDWR);
      if (mDMAHeapDeaviceFD < 0) {
         result = -1;
         VIDC_ERR("DisplayAdaptor::createFrameBuffer, failed to open DMA: errno: %d, %s\n", errno, strerror(errno));
         return result;
      }
   }
   uint32_t uvOffset = dataFrameWidth * dataFrameHeight;
   drm_buffer_context drmBufferContext = {};
   struct dma_heap_allocation_data allocationData;
   memset(&allocationData, 0, sizeof(allocationData));
   allocationData.len = dataFrameSize;
   allocationData.fd_flags = O_RDWR | O_CLOEXEC;
   allocationData.heap_flags = 0;
   result = ioctl(mDMAHeapDeaviceFD, DMA_HEAP_IOCTL_ALLOC, &allocationData);
   if (result < 0) {
      VIDC_ERR("DisplayAdaptor::createFrameBuffer, failed to alloc buffer: errno: %d, %s\n", errno, strerror(errno));
      return result;
   }
   drmBufferContext.dma_heap_fd = allocationData.fd;
   result = drmPrimeFDToHandle(mDisplayCardFD, drmBufferContext.dma_heap_fd, &(drmBufferContext.gem_handle));
   if (result < 0) {
      close(drmBufferContext.dma_heap_fd);
      VIDC_ERR("DisplayAdaptor::createFrameBuffer, failed to transfer dma buffer: errno: %d, %s\n", errno, strerror(errno));
      return result;
   }
   uint32_t handles[4] = {drmBufferContext.gem_handle, drmBufferContext.gem_handle, 0, 0};
   uint32_t strides[4] = {dataFrameWidth, dataFrameWidth, 0, 0};//stride parameters will be configured by drm for UBWC format.
   uint32_t offsets[4] = {0, uvOffset, 0, 0};//offset parameters will be configured by drm for UBWC format.
   uint64_t modifiers[4] = {FrameBuferModifier, FrameBuferModifier, 0, 0};
   result = drmModeAddFB2WithModifiers(mDisplayCardFD, dataFrameWidth, dataFrameHeight, PixelFormat,
         handles, strides, offsets, modifiers, &(drmBufferContext.fb_id), DRM_MODE_FB_MODIFIERS);
   if (result < 0) {
      ioctl(mDisplayCardFD, DRM_IOCTL_GEM_CLOSE, &(drmBufferContext.gem_handle));
      close(drmBufferContext.dma_heap_fd);
      VIDC_ERR("DisplayAdaptor::createFrameBuffer, failed to addFB2: result: %d, errno %d, %s\n",
         result, errno, strerror(errno));
      return result;
   }
   VIDC_MED("DisplayAdaptor::createFrameBuffer, drmBufferContext.dma_heap_fd = %d, drmBufferContext.gem_handle = %d, drmBufferContext.fb_id = %d\n",
      drmBufferContext.dma_heap_fd, drmBufferContext.gem_handle, drmBufferContext.fb_id);

   for (int i = 0; i < (int)mPlaneCfgVector.size(); i++) {
      mPlaneCfgVector[i].fb.ptr = mmap(NULL, dataFrameSize, PROT_READ | PROT_WRITE, MAP_SHARED, drmBufferContext.dma_heap_fd, 0);
      if (mPlaneCfgVector[i].fb.ptr == MAP_FAILED) {
         mPlaneCfgVector[i].fb.ptr = NULL;
         VIDC_ERR("DisplayAdaptor::createFrameBuffer, failed to map buffer: errno %d, %s\n", errno, strerror(errno));
      }
      else {
         mPlaneCfgVector[i].fb.size = dataFrameSize;
         fb_obj fbObj = {};
         fbObj.ptr = mPlaneCfgVector[i].fb.ptr;
         fbObj.size = mPlaneCfgVector[i].fb.size;
         drmBufferContext.fb_objs.push_back(fbObj);
      }
      mPlaneCfgVector[i].fb.fb_id = drmBufferContext.fb_id;
   }
   mLastDRMBufferContextVector.push_back(drmBufferContext);
   mLastDrmBufferContext = &mLastDRMBufferContextVector[mLastDRMBufferContextVector.size() - 1];
   return result;
}

bool DisplayAdaptor::updateFrameBuffer(int connectorCfgIndex, int& drmOutFenceFD) {
   VIDC_MED("DisplayAdaptor::updateFrameBuffer, connectorCfgIndex = %d\n", connectorCfgIndex);
   bool result = true;
   for (int i = 0; i < (int)mPlaneCfgVector.size(); i++) {
      VIDC_MED("DisplayAdaptor::updateFrameBuffer, i = %d\n", i);
      if (mPlaneCfgVector[i].crtc_id != mConnectorCfgVector[connectorCfgIndex].crtc_id) {
         VIDC_MED("DisplayAdaptor::updateFrameBuffer, mPlaneCfgVector[%d].crtc_id = %d, mConnectorCfgVector[%d].crtc_id = %d\n",
            i, mPlaneCfgVector[i].crtc_id, connectorCfgIndex, mConnectorCfgVector[connectorCfgIndex].crtc_id);
         continue;
      }

      /* update plane in locked context */
      std::lock_guard<std::mutex> guard(planeMutex);
      /* check possible_crtcs */
      if (!(mPlaneCfgVector[i].possible_crtcs & (1 << mConnectorCfgVector[connectorCfgIndex].crtc_idx))) {
         VIDC_MED("DisplayAdaptor::updateFrameBuffer, mPlaneCfgVector[%d].possible_crtcs = %d, mConnectorCfgVector[%d].crtc_idx) = %d\n",
            i, mPlaneCfgVector[i].possible_crtcs, connectorCfgIndex, mConnectorCfgVector[connectorCfgIndex].crtc_idx);
         continue;
      }
      /* first run */
      if (mPlaneCfgVector[i].first_run) {
         /*update crtc_id and plane_id*/
         VIDC_HIGH("DisplayAdaptor::updateFrameBuffer, update render parameters for plane %d\n", i);
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].crtc_pid, mPlaneCfgVector[i].crtc_id);
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].zpos_pid, mPlaneCfgVector[i].zpos);
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].crtc_x_pid, mPlaneCfgVector[i].x);
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].crtc_y_pid, mPlaneCfgVector[i].y);
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].crtc_w_pid, mPlaneCfgVector[i].w);
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].crtc_h_pid, mPlaneCfgVector[i].h);
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].src_y_pid, 0 << 16);
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].src_w_pid, mPlaneCfgVector[i].w << 16);
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].src_h_pid, mPlaneCfgVector[i].h << 16);
         mPlaneCfgVector[i].first_run = false;
         /* only set handoff once */
         if (mPlaneCfgVector[i].handoff_set && mPlaneCfgVector[i].handoff_pid) {
            drmModeAtomicAddProperty(
               mDRMModeReqPtr,  mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].handoff_pid, mPlaneCfgVector[i].handoff);
            mPlaneCfgVector[i].handoff_set = false;
         }

         //get fence property id
         getPropId(mPlaneCfgVector[i].crtc_id, DRM_MODE_OBJECT_CRTC, "OUT_FENCE_PTR", &mDRMOutFencePtrPropID);

         //scale and fit image to screen center
         uint32_t crtcActive = 0, crtcModeId = 0, connecotrCrtcId = 0;
         uint32_t planeFrameBuferId = 0, planeCrtcId = 0, planeSrcX = 0, planeSrcY = 0, planeSrcWidth = 0, planeSrcHeight = 0;
         uint32_t planeCrtcX = 0, planeCrtcY = 0, planeCrtcWidth = 0, planeCrtcHeight = 0;
         uint32_t planeZPosition = 0; //optional

         getPropId(mPlaneCfgVector[i].crtc_id, DRM_MODE_OBJECT_CRTC, "ACTIVE", &crtcActive);
         getPropId(mPlaneCfgVector[i].crtc_id, DRM_MODE_OBJECT_CRTC, "MODE_ID", &crtcModeId);
         getPropId(mPlaneCfgVector[i].connector_id, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID", &connecotrCrtcId);

         getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "FB_ID", &planeFrameBuferId);
         getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_ID", &planeCrtcId);
         getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "SRC_X", &planeSrcX);
         getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "SRC_Y", &planeSrcY);
         getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "SRC_W", &planeSrcWidth);
         getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "SRC_H", &planeSrcHeight);
         getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_X", &planeCrtcX);
         getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_Y", &planeCrtcY);
         getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_W", &planeCrtcWidth);
         getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "CRTC_H", &planeCrtcHeight);

         //create MODE_ID blob
         uint32_t mode_blob_id = 0;
         drmModeCreatePropertyBlob(
            mDisplayCardFD, (const void*)&mConnectorCfgVector[connectorCfgIndex].mode, sizeof(drmModeModeInfo), &mode_blob_id);

         int screenWidth = mConnectorCfgVector[connectorCfgIndex].mode.hdisplay;
         int screenHeight = mConnectorCfgVector[connectorCfgIndex].mode.vdisplay;
         double ratioW = (double)screenWidth / mSourceImgWidth;
         double ratioH = (double)screenHeight / mSourceImgHeight;
         if (ratioH < ratioW) {
            ratioW = ratioH;
         }
         int dstWidth = (int)(mSourceImgWidth * ratioW + 0.5);
         int dstHeight = (int)(mSourceImgHeight * ratioW + 0.5);
         if (dstWidth & 1) {
            dstWidth--;
         }
         if (dstHeight & 1) {
            dstHeight--;
         }
         int dstX = (screenWidth - dstWidth) / 2;
         int dstY = (screenHeight - dstHeight) / 2;
         VIDC_MED("DisplayAdaptor::updateFrameBuffer, i = %d, dstX = %d, dstY = %d, dstWidth = %d, dstHeight = %d\n",
            i, dstX, dstY, dstWidth, dstHeight);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].crtc_id, crtcActive, 1);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].crtc_id, crtcModeId, mode_blob_id);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].connector_id, connecotrCrtcId, mPlaneCfgVector[i].crtc_id);

         // Plane：SRC use 16.16 fix format
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeFrameBuferId, mPlaneCfgVector[i].fb.fb_id);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeCrtcId, mPlaneCfgVector[i].crtc_id);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeSrcX, FIXED_16_16(0));
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeSrcY, FIXED_16_16(0));
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeSrcWidth, FIXED_16_16(mSourceImgWidth));
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeSrcHeight, FIXED_16_16(mSourceImgHeight));
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeCrtcX, dstX);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeCrtcY, dstY);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeCrtcWidth, dstWidth);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeCrtcHeight, dstHeight);

         //optional：zpos and rotation(if exists)
         if (getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "ZPOS", &planeZPosition) == 0) {
            drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeZPosition, 1); //make to top layer
         }

         //if need rotate：DRM_MODE_ROTATE_0/90/180/270（depends on driver）
         // uint32_t rot_prop;
         // if (getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "rotation", &rot_prop) == 0) {
         //    drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, rot_prop, DRM_MODE_ROTATE_90);
         // }

         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].crtc_id, mDRMOutFencePtrPropID, (uint64_t)(uintptr_t)&drmOutFenceFD);
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].fb_pid, mPlaneCfgVector[i].fb.fb_id);
         result &= atomicCommit();
         drmModeDestroyPropertyBlob(mDisplayCardFD, mode_blob_id);
         mPlaneCfgVector[i].first_run = false;
      }
      else {
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].crtc_id, mDRMOutFencePtrPropID, (uint64_t)(uintptr_t)&drmOutFenceFD);
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].crtc_pid, mPlaneCfgVector[i].crtc_id);
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].fb_pid, mPlaneCfgVector[i].fb.fb_id);
         result &= atomicCommit();
      }
   }
   return result;
}

int DisplayAdaptor::setupConnector() {
   VIDC_MED("DisplayAdaptor::setupConnector, enter\n");
   for (int i = 0; i < (int)mConnectorCfgVector.size(); i++) {
      /* set mode */
      uint32_t blobId;

      int resultNo = drmModeCreatePropertyBlob(mDisplayCardFD, (const void*)&mConnectorCfgVector[i].mode, sizeof(drmModeModeInfo), &blobId);
      if (resultNo != 0) {
         VIDC_ERR("DisplayAdaptor::setupConnector, failed to create mode blob\n");
         return -1;
      }

      drmModeAtomicAddProperty(mDRMModeReqPtr, mConnectorCfgVector[i].crtc_id, mConnectorCfgVector[i].mode_id_pid, blobId);
      /* set active */
      drmModeAtomicAddProperty(mDRMModeReqPtr, mConnectorCfgVector[i].crtc_id, mConnectorCfgVector[i].active_pid, 1);
      /* set crtc */
      drmModeAtomicAddProperty(
         mDRMModeReqPtr, mConnectorCfgVector[i].connector_id, mConnectorCfgVector[i].crtc_id_pid, mConnectorCfgVector[i].crtc_id);

      /* init commit */
      bool result = atomicCommit();
      if (!result) {
         VIDC_ERR("DisplayAdaptor::setupConnector, commit failed\n");
         return result;
      }
      VIDC_MED("DisplayAdaptor::setupConnector, commit success\n");
   }
   return 0;
}

int DisplayAdaptor::getPropId(uint32_t objId, uint32_t objType, const char *name, uint32_t *propIdOut) {
   drmModeObjectProperties* propertiesPtr = drmModeObjectGetProperties(mDisplayCardFD, objId, objType);
   if (propertiesPtr == NULL) {
      return -1;
   }
   int result = -1;
   for (uint32_t i = 0; i < propertiesPtr->count_props; ++i) {
      drmModePropertyRes* propertyRes = drmModeGetProperty(mDisplayCardFD, propertiesPtr->props[i]);
      if (propertyRes != NULL && strcmp(propertyRes->name, name) == 0) {
         *propIdOut = propertyRes->prop_id;
         result = 0;
         drmModeFreeProperty(propertyRes);
         break;
      }
      if (propertyRes != NULL) {
         drmModeFreeProperty(propertyRes);
      }
   }
   drmModeFreeObjectProperties(propertiesPtr);
   return result;
}

bool DisplayAdaptor::atomicCommit() {
   int flags = DRM_MODE_ATOMIC_ALLOW_MODESET | DRM_MODE_ATOMIC_NONBLOCK;
	int result = drmModeAtomicCommit(mDisplayCardFD, mDRMModeReqPtr, flags, 0);
	drmModeAtomicSetCursor(mDRMModeReqPtr, 0);
	if (result != 0) {
      VIDC_ERR("DisplayAdaptor::atomicCommit, commit failed: result: %d, errno: %d, %s\n",
         result, errno, strerror(errno));
		return false;
   }
	return true;
}

void DisplayAdaptor::waitDRMOutFenceAndReset(int& fenceFD, int timeoutMS) {
   if (fenceFD < 0) {
      return;
   }
   struct pollfd pfd;
   pfd.fd = fenceFD;
   pfd.events = POLLIN;
   pfd.revents = 0;
   int result = poll(&pfd, 1, timeoutMS);
   VIDC_LOW("DisplayAdaptor::waitDRMOutFenceAndReset, fenceFD = %d, result = %d\n", fenceFD, result);
   close(fenceFD);
   fenceFD = -1;
}

void DisplayAdaptor::updatePossibleCrtcs(void) {
   planeMutex.lock();
   /* update possible_crtcs on errors */
   for (int i = 0; i < mPlaneCfgVector.size(); i++) {
      drmModePlanePtr planePtr = drmModeGetPlane(mDisplayCardFD, mPlaneCfgVector[i].plane_id);
      if (planePtr != NULL && mPlaneCfgVector[i].possible_crtcs != planePtr->possible_crtcs) {
         VIDC_MED("DisplayAdaptor::updatePossibleCrtcs, plane %d possible_crtcs 0x%x => 0x%x\n",
            mPlaneCfgVector[i].plane_id,
            mPlaneCfgVector[i].possible_crtcs,
            planePtr->possible_crtcs);
         mPlaneCfgVector[i].possible_crtcs = planePtr->possible_crtcs;
         mPlaneCfgVector[i].first_run = true;
      }
      drmModeFreePlane(planePtr);
   }
   planeMutex.unlock();
}

void DisplayAdaptor::destroyBuff(int keepLatestItemCount) {
   VIDC_LOW("DisplayAdaptor::destroyBuff, keepLatestItemCount = %d, mLastDRMBufferContextVector.size = %d\n",
      keepLatestItemCount, mLastDRMBufferContextVector.size());
   if (keepLatestItemCount < 0) {
      keepLatestItemCount = 0;
   }
   if (keepLatestItemCount >= mLastDRMBufferContextVector.size()) {
      return;
   }

   for(int i = 0; i < mLastDRMBufferContextVector.size() - keepLatestItemCount; i++) {
      drm_buffer_context& drmBufferContext = mLastDRMBufferContextVector[i];
      VIDC_LOW("DisplayAdaptor::destroyBuff, drmBufferContext.fb_id = %d, drmBufferContext.gem_handle = %d, drmBufferContext.dma_heap_fd = %d\n",
         drmBufferContext.fb_id, drmBufferContext.gem_handle, drmBufferContext.dma_heap_fd);
      if (drmBufferContext.fb_id >= 0) {
         drmModeRmFB(mDisplayCardFD, drmBufferContext.fb_id);
      }
      if (drmBufferContext.gem_handle >= 0) {
         ioctl(mDisplayCardFD, DRM_IOCTL_GEM_CLOSE, &(drmBufferContext.gem_handle));
      }
      if (drmBufferContext.dma_heap_fd >= 0) {
         close(drmBufferContext.dma_heap_fd);
      }
      VIDC_LOW("DisplayAdaptor::destroyBuff, drmBufferContext.fb_objs.size = %d\n", drmBufferContext.fb_objs.size());
      for(fb_obj& fbObj : drmBufferContext.fb_objs) {
         if (fbObj.ptr != NULL) {
            munmap(fbObj.ptr, fbObj.size);
         }
      }
      drmBufferContext.fb_objs.clear();
      VIDC_LOW("DisplayAdaptor::destroyBuff, drmBufferContext.drm_fence_fds.size = %d\n", drmBufferContext.drm_fence_fds.size());
      for(int fenceFD : drmBufferContext.drm_fence_fds) {
         if (fenceFD >= 0) {
            close(fenceFD);
         }
      }
      drmBufferContext.drm_fence_fds.clear();
   }
   mLastDRMBufferContextVector.erase(mLastDRMBufferContextVector.begin(), mLastDRMBufferContextVector.end() - keepLatestItemCount);
   if (keepLatestItemCount <= 0) {
      mLastDrmBufferContext = NULL;
   }
}
