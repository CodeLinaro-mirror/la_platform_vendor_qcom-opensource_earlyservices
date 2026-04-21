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

#include <linux/ioctl.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <cutils/properties.h>

#include "DisplayAdaptor.h"
#include "VidcLog.h"

#define MAX_COUNT             2

#define FIXED_16_16(x)        ((uint64_t)(x) << 16)
#define ALIGN(v, a) (((a) & ((a) - 1)) ?\
   ((((v) + (a) - 1) / (a)) * (a)) :\
   (((v) + (a) - 1) & (~((a) - 1))))

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
const char* DisplayCardPath = "/dev/dri/card5";
const uint64_t PixelFormat = DRM_FORMAT_NV12;
//0(when not DRM_FORMAT_MOD_QCOM_COMPRESSED), DRM_FORMAT_MOD_QCOM_COMPRESSED
const uint64_t FrameBufferModifier = DRM_FORMAT_MOD_QCOM_COMPRESSED;

std::mutex initMutex;
std::mutex planeMutex;
std::atomic<bool> isAdaptorInitialized{false};

using namespace early_video_app;

DisplayAdaptor::DisplayAdaptor() :
   mDisplayCardFD(-1),
   mDMAHeapDeviceFD(-1),
   mDRMModeReqPtr(NULL),
   mCurrentFrameBufferIndex(0) {

}

bool DisplayAdaptor::isInitialized() {
   return isAdaptorInitialized.load();
}

bool DisplayAdaptor::initAdaptor(uint32_t frameWidth, uint32_t frameHeight, uint32_t dataSize) {
   initMutex.lock();
   if (isAdaptorInitialized.load()) {
      bool result = (frameWidth == mSourceFrameWidth && frameHeight == mSourceFrameHeight);
      initMutex.unlock();
      return result;
   }
   VIDC_MED("DisplayAdaptor::initAdaptor, frameWidth = %d, frameHeight = %d\n", frameWidth, frameHeight);
   VIDC_MED("DisplayAdaptor::initAdaptor, open display card: %s\n", DisplayCardPath);
   mDisplayCardFD = open(DisplayCardPath, O_RDWR, 0);
   if (mDisplayCardFD < 0) {
      VIDC_ERR("DisplayAdaptor::initAdaptor, failed to open %s\n", DisplayCardPath);
      initMutex.unlock();
      return false;
   }

   drmSetClientCap(mDisplayCardFD, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
   drmSetClientCap(mDisplayCardFD, DRM_CLIENT_CAP_ATOMIC, 1);

   VIDC_MED("DisplayAdaptor::initAdaptor, parse display\n");
   int result = parseDisplay();
   if (result != 0) {
      VIDC_ERR("DisplayAdaptor::initAdaptor, parse_display failed\n");
      initMutex.unlock();
      return false;
   }

   VIDC_MED("DisplayAdaptor::initAdaptor, create frame buffer\n");
   if (PixelFormat == DRM_FORMAT_NV12) {
      if (FrameBufferModifier == DRM_FORMAT_MOD_QCOM_COMPRESSED) {
         result = createFrameBufferForNV12UBWC(frameWidth, frameHeight, dataSize);
      }
      else {
         result = createFrameBufferForNV12(frameWidth, frameHeight);
      }
   }
   else {
      VIDC_ERR("DisplayAdaptor::initAdaptor, not supported format 0x%x except NV12 and NV12UBWC\n", PixelFormat);
      initMutex.unlock();
      return false;
   }
   if (result != 0) {
      VIDC_ERR("DisplayAdaptor::initAdaptor, Failed to create framebuffer\n");
      initMutex.unlock();
      return false;
   }

   /* init atomic commit */
   VIDC_MED("DisplayAdaptor::initAdaptor, alloc atomic commit\n");
   mDRMModeReqPtr = drmModeAtomicAlloc();
   if (mDRMModeReqPtr == NULL) {
      VIDC_ERR("DisplayAdaptor::initAdaptor, failed to init atomic commit\n");
      initMutex.unlock();
      return false;
   }

   /* setup crtc / connector */
   VIDC_MED("DisplayAdaptor::initAdaptor, setup connector\n");
   result = setupConnector();
   if (result != 0) {
      VIDC_ERR("DisplayAdaptor::initAdaptor, failed to setup connector\n");
      initMutex.unlock();
      return false;
   }
   VIDC_MED("DisplayAdaptor::initAdaptor, setup connector successfully\n");
   mSourceFrameWidth = frameWidth;
   mSourceFrameHeight = frameHeight;
   isAdaptorInitialized.store(true);
   initMutex.unlock();

   return true;
}

bool DisplayAdaptor::commitData(std::uint8_t* pBuffer, uint32_t length, uint32_t offset) {
   if (!isAdaptorInitialized.load()) {
      VIDC_MED("DisplayAdaptor::commitData, adaptor not initialized\n");
      return false;
   }

   if (mCurrentFrameBufferIndex >= MAX_BUFFER) {
      mCurrentFrameBufferIndex = 0;
   }
   VIDC_MED("DisplayAdaptor::commitData, enter, length = %d\n", length);
   if (mPlaneCfgVector[0].fb[mCurrentFrameBufferIndex].ptr != NULL) {
      memcpy(mPlaneCfgVector[0].fb[mCurrentFrameBufferIndex].ptr, pBuffer + offset, length);
   }

   VIDC_MED("DisplayAdaptor::commitData, =======update fb and commit fb========\n");
   for (int i = 0; i < (int) mConnectorCfgVector.size(); i++) {
      /* update fb */
      bool result = updateFrameBuffer(i, mCurrentFrameBufferIndex);
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
   }
   mCurrentFrameBufferIndex++;
   return true;
}

bool DisplayAdaptor::deinitAdaptor() {
   if (!isAdaptorInitialized.load()) {
      return 0;
   }
   if (mDRMModeReqPtr != NULL) {
      drmModeAtomicFree(mDRMModeReqPtr);
   }

   /* destroy buffer */
   int result = destroyBuff();
   if (result != 0) {
      return result;
   }

   /* close fd */
   if (mDisplayCardFD >= 0) {
      close(mDisplayCardFD);
      mDisplayCardFD = -1;
   }
   if (mDMAHeapDeviceFD >= 0) {
      close(mDMAHeapDeviceFD);
      mDMAHeapDeviceFD = -1;
   }
   isAdaptorInitialized.store(false);
   return 0;
}

int DisplayAdaptor::parseDisplay(void) {
   int connectorIndex = 0;
   int crtcIndex = 0;
   int planeIndex = 0;
   connector_config connectorCfgArray[MAX_COUNT] = {};
   plane_config planeCfg = {};
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
         VIDC_MED("DisplayAdaptor::parseDisplay, connector: %d not connected\n", i);
         continue;
      }

      /* dump */
      VIDC_MED("DisplayAdaptor::parseDisplay, connectorIndex %d: %d - %d-%d crtcs:",
         connectorIndex, connectorPtr->connector_id,
         connectorPtr->connector_type,
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
            VIDC_MED("DisplayAdaptor::parseDisplay, crtcs[%d] = %d", j, displayRes->crtcs[j]);
         }
      }
      for (int j = 0; j < connectorPtr->count_modes; j++) {
         VIDC_MED("DisplayAdaptor::parseDisplay, connectors[%d]->modes[%d]%s", i, j, connectorPtr->modes[j].name);
      }
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
         VIDC_MED("DisplayAdaptor::parseDisplay, colorFormat: 0x%08x\n", colorFormat);
      }

      VIDC_MED("DisplayAdaptor::parseDisplay, plane %d: %d - crtcs:", i, planePtr->plane_id);
      for (int j = 0; j < displayRes->count_crtcs; j++) {
         if (planePtr->possible_crtcs & (1 << j) && displayRes->crtcs) {
            VIDC_MED("DisplayAdaptor::parseDisplay, crtc: %d", displayRes->crtcs[j]);
         }
      }
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

int DisplayAdaptor::createFrameBufferForNV12(uint32_t frameWidth, uint32_t frameHeight) {
   VIDC_MED("DisplayAdaptor::createFrameBufferForNV12, frameWidth = %d, frameHeight = %d\n", frameWidth, frameHeight);
   int result = 0;
   /* create framebuffer */
   mDMAHeapDeviceFD = open(DMADevicePath, O_RDWR);
   if (mDMAHeapDeviceFD < 0) {
      result = -1;
      VIDC_ERR("DisplayAdaptor::createFrameBufferForNV12, failed to open DMA: errno: %d, %s\n", errno, strerror(errno));
      return result;
   }
   for (int j = 0; j < MAX_BUFFER; j++) {
      int stride = ALIGN(frameWidth, 128);
      int ySize = stride * frameHeight;
      int uvSize = stride * frameHeight / 2;
      int totalSize = ySize + uvSize;

      struct dma_heap_allocation_data allocationData;
      memset(&allocationData, 0, sizeof(allocationData));
      allocationData.len = totalSize;
      allocationData.fd_flags = O_RDWR | O_CLOEXEC;
      allocationData.heap_flags = 0;
      result = ioctl(mDMAHeapDeviceFD, DMA_HEAP_IOCTL_ALLOC, &allocationData);
      if (result < 0) {
         VIDC_ERR("DisplayAdaptor::createFrameBufferForNV12, failed to alloc buffer: errno: %d, %s\n", errno, strerror(errno));
         return result;
      }

      int allocatedDMAFD = allocationData.fd;
      uint32_t gemHandle;
      result = drmPrimeFDToHandle(mDisplayCardFD, allocatedDMAFD, &gemHandle);
      if (result < 0) {
         VIDC_ERR("DisplayAdaptor::createFrameBufferForNV12, failed to transfer dma buffer: errno: %d, %s\n", errno, strerror(errno));
         return result;
      }

      struct drm_mode_fb_cmd2 frameBufferCmd2[MAX_BUFFER]{};
      frameBufferCmd2[j].width = frameWidth;
      frameBufferCmd2[j].height = frameHeight;
      frameBufferCmd2[j].pixel_format = PixelFormat;
      frameBufferCmd2[j].flags = DRM_MODE_FB_MODIFIERS;

      frameBufferCmd2[j].handles[0] = gemHandle;
      frameBufferCmd2[j].pitches[0] = stride;
      frameBufferCmd2[j].offsets[0] = 0;

      frameBufferCmd2[j].handles[1] = gemHandle;
      frameBufferCmd2[j].pitches[1] = stride;
      frameBufferCmd2[j].offsets[1] = ySize;

      frameBufferCmd2[j].modifier[0] = FrameBufferModifier;
      frameBufferCmd2[j].modifier[1] = FrameBufferModifier;

      result = drmIoctl(mDisplayCardFD, DRM_IOCTL_MODE_ADDFB2, &frameBufferCmd2[j]);
      if (result != 0) {
         VIDC_ERR("DisplayAdaptor::createFrameBufferForNV12, failed to addfb2: result: %d, errno %d, %s\n",
            result, errno, strerror(errno));
         return result;
      }
      for (int i = 0; i < (int)mPlaneCfgVector.size(); i++) {
         mPlaneCfgVector[i].fb[j].ptr = mmap(NULL, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, allocatedDMAFD, 0);
         mPlaneCfgVector[i].fb[j].size = totalSize; 
         mPlaneCfgVector[i].fb[j].fb_id = frameBufferCmd2[j].fb_id;
      }
   }
   /* first run flag */
   for (int i = 0; i < (int)mPlaneCfgVector.size(); i++) {
      mPlaneCfgVector[i].first_run = true;
   }

   return result;
}

int DisplayAdaptor::createFrameBufferForNV12UBWC(uint32_t frameWidth, uint32_t frameHeight, uint32_t dataSize) {
   VIDC_MED("DisplayAdaptor::createFrameBufferForNV12UBWC, frameWidth = %d, frameHeight = %d, dataSize = %d\n",
      frameWidth, frameHeight, dataSize);
   int result = 0;
   /* create framebuffer */
   mDMAHeapDeviceFD = open(DMADevicePath, O_RDWR);
   if (mDMAHeapDeviceFD < 0) {
      result = -1;
      VIDC_ERR("DisplayAdaptor::createFrameBufferForNV12UBWC, failed to open DMA: errno: %d, %s\n", errno, strerror(errno));
      return result;
   }

   for (int j = 0; j < MAX_BUFFER; j++) {
      struct dma_heap_allocation_data allocationData;
      memset(&allocationData, 0, sizeof(allocationData));
      allocationData.len = dataSize;
      allocationData.fd_flags = O_RDWR | O_CLOEXEC;
      allocationData.heap_flags = 0;
      result = ioctl(mDMAHeapDeviceFD, DMA_HEAP_IOCTL_ALLOC, &allocationData);
      if (result < 0) {
         VIDC_ERR("DisplayAdaptor::createFrameBufferForNV12UBWC, failed to alloc buffer: errno: %d, %s\n", errno, strerror(errno));
         return result;
      }

      int allocatedDMAFD = allocationData.fd;
      uint32_t gemHandle;
      result = drmPrimeFDToHandle(mDisplayCardFD, allocatedDMAFD, &gemHandle);
      if (result < 0) {
         VIDC_ERR("DisplayAdaptor::createFrameBufferForNV12UBWC, failed to transfer dma buffer: errno: %d, %s\n", errno, strerror(errno));
         return result;
      }

      uint32_t handles[4] = {gemHandle, gemHandle, 0, 0};
      uint32_t strides[4] = {frameWidth, frameWidth, 0, 0};//stride parameters will be configured by drm for UBWC format.
      uint32_t offsets[4] = {0, 0, 0, 0};//offset parameters will be configured by drm for UBWC format.
      uint64_t modifiers[4] = {FrameBufferModifier, FrameBufferModifier, 0, 0};
      uint32_t frameBufferID = 0;
      result = drmModeAddFB2WithModifiers(mDisplayCardFD, frameWidth, frameHeight, PixelFormat,
            handles, strides, offsets, modifiers, &frameBufferID, DRM_MODE_FB_MODIFIERS);
      if (result < 0) {
         VIDC_ERR("DisplayAdaptor::createFrameBufferForNV12UBWC, failed to addFB2: result: %d, errno %d, %s\n",
            result, errno, strerror(errno));
         return result;
      }

      for (int i = 0; i < (int)mPlaneCfgVector.size(); i++) {
         mPlaneCfgVector[i].fb[j].ptr = mmap(NULL, dataSize, PROT_READ | PROT_WRITE, MAP_SHARED, allocatedDMAFD, 0);
         if (mPlaneCfgVector[i].fb[j].ptr == NULL) {
            VIDC_ERR("DisplayAdaptor::createFrameBufferForNV12UBWC, failed to map buffer: j: %d, errno %d, %s\n", j, errno, strerror(errno));
         }
         mPlaneCfgVector[i].fb[j].size = dataSize;
         mPlaneCfgVector[i].fb[j].fb_id = frameBufferID;
      }
   }
   /* first run flag */
   for (int i = 0; i < (int)mPlaneCfgVector.size(); i++) {
      mPlaneCfgVector[i].first_run = true;
   }

   return result;
}

bool DisplayAdaptor::updateFrameBuffer(int connectorCfgIndex, int bufIndex) {
   VIDC_MED("DisplayAdaptor::updateFrameBuffer, connectorCfgIndex = %d, bufIndex = %d\n", connectorCfgIndex, bufIndex);
   bool result = true;
   for (int i = 0; i < (int)mPlaneCfgVector.size(); i++) {
      VIDC_MED("DisplayAdaptor::updateFrameBuffer, i = %d\n", i);
      if (mPlaneCfgVector[i].crtc_id != mConnectorCfgVector[connectorCfgIndex].crtc_id) {
         continue;
      }

      /* update plane in locked context */
      std::lock_guard<std::mutex> guard(planeMutex);
      /* check possible_crtcs */
      if (!(mPlaneCfgVector[i].possible_crtcs & (1 << mConnectorCfgVector[connectorCfgIndex].crtc_idx))) {
         VIDC_MED("DisplayAdaptor::updateFrameBuffer, i = %d, plane cfg crtc not possible\n", i);
         continue;
      }
      /* first run */
      if (mPlaneCfgVector[i].first_run) {
         /*update crtc_id and plane_id*/
         VIDC_HIGH("DisplayAdaptor::initAdaptor, update render parameters for plane %d\n", i);
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
         drmModeCreatePropertyBlob(mDisplayCardFD, (const void*)&mConnectorCfgVector[i].mode, sizeof(drmModeModeInfo), &mode_blob_id);

         int screenWidth = mConnectorCfgVector[i].mode.hdisplay;
         int screenHeight = mConnectorCfgVector[i].mode.vdisplay;
         double ratioW = (double)screenWidth / mSourceFrameWidth;
         double ratioH = (double)screenHeight / mSourceFrameHeight;
         if (ratioH < ratioW) {
            ratioW = ratioH;
         }
         int dstWidth = (int)(mSourceFrameWidth * ratioW + 0.5);
         int dstHeight = (int)(mSourceFrameHeight * ratioW + 0.5);
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
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeFrameBuferId, mPlaneCfgVector[i].fb[bufIndex].fb_id);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeCrtcId, mPlaneCfgVector[i].crtc_id);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeSrcX, FIXED_16_16(0));
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeSrcY, FIXED_16_16(0));
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeSrcWidth, FIXED_16_16(mSourceFrameWidth));
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeSrcHeight, FIXED_16_16(mSourceFrameHeight));
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeCrtcX, dstX);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeCrtcY, dstY);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeCrtcWidth, dstWidth);
         drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeCrtcHeight, dstHeight);

         //optional：zpos and rotation(if exists)
         if (getPropId(mPlaneCfgVector[i].plane_id, DRM_MODE_OBJECT_PLANE, "ZPOS", &planeZPosition) == 0) {
            drmModeAtomicAddProperty(mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, planeZPosition, 1); //make to top layer
         }

         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].fb_pid, mPlaneCfgVector[i].fb[bufIndex].fb_id);
         result &= atomicCommit(true);
         drmModeDestroyPropertyBlob(mDisplayCardFD, mode_blob_id);
      }
      else {
         drmModeAtomicAddProperty(
            mDRMModeReqPtr, mPlaneCfgVector[i].plane_id, mPlaneCfgVector[i].fb_pid, mPlaneCfgVector[i].fb[bufIndex].fb_id);
         result &= atomicCommit(true);
      }
   }
   return result;
}

int DisplayAdaptor::setupConnector() {
   for (int i = 0; i < (int)mConnectorCfgVector.size(); i++) {
      /* set mode */
      uint32_t blobId;

      if (drmModeCreatePropertyBlob(mDisplayCardFD, (const void*) &mConnectorCfgVector[i].mode, sizeof(drmModeModeInfo), &blobId)) {
         VIDC_ERR("DisplayAdaptor::setupConnector, failed to create mode blob\n");
         return 0;
      }

      drmModeAtomicAddProperty(mDRMModeReqPtr, mConnectorCfgVector[i].crtc_id, mConnectorCfgVector[i].mode_id_pid, blobId);
      /* set active */
      drmModeAtomicAddProperty(mDRMModeReqPtr, mConnectorCfgVector[i].crtc_id, mConnectorCfgVector[i].active_pid, 1);
      /* set crtc */
      drmModeAtomicAddProperty(
         mDRMModeReqPtr, mConnectorCfgVector[i].connector_id, mConnectorCfgVector[i].crtc_id_pid, mConnectorCfgVector[i].crtc_id);

      /* init commit */
      bool result = atomicCommit(false);
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

bool DisplayAdaptor::atomicCommit(bool isAsync) {
   int flags = DRM_MODE_ATOMIC_ALLOW_MODESET | (isAsync ? DRM_MODE_ATOMIC_NONBLOCK : 0);
   int result = drmModeAtomicCommit(mDisplayCardFD, mDRMModeReqPtr, flags, 0);
   drmModeAtomicSetCursor(mDRMModeReqPtr, 0);
   if (result != 0) {
      VIDC_ERR("DisplayAdaptor::atomicCommit, commit failed: result: %d, errno: %d, %s\n",
         result, errno, strerror(errno));
      return false;
   }
   return true;
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

int DisplayAdaptor::destroyBuff() {
   int result = 0;

   for (int planeIdx = 0; planeIdx < (int)mPlaneCfgVector.size(); planeIdx++) {
      for (int bufIdx = 0; bufIdx < MAX_BUFFER; bufIdx++) {

         if (mPlaneCfgVector[planeIdx].fb[bufIdx].ptr != nullptr && 
             mPlaneCfgVector[planeIdx].fb[bufIdx].size > 0) {
            if (munmap(mPlaneCfgVector[planeIdx].fb[bufIdx].ptr, 
                      mPlaneCfgVector[planeIdx].fb[bufIdx].size) != 0) {
               VIDC_ERR("DisplayAdaptor::destroyBuff, munmap failed for plane %d, buffer %d: errno %d, %s\n",
                       planeIdx, bufIdx, errno, strerror(errno));
               result = -1;
            }

            mPlaneCfgVector[planeIdx].fb[bufIdx].ptr = nullptr;
            mPlaneCfgVector[planeIdx].fb[bufIdx].size = 0;
         }
      }
   }

   return result;
}
