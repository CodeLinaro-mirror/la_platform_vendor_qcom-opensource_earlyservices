/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef _LINUX_VENV_
#include <cutils/properties.h>
#else
#define PROPERTY_VALUE_MAX 128
#endif

#include <cstddef>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <chrono>
#include <filesystem>
#include <cstdio>
#include <stdlib.h>
#include <string.h>
#include <mutex>
#include <android-base/logging.h>

#include "VidcLog.h"

namespace early_video_app {

    FILE* mInnerLogFile = NULL;
    std::mutex mLogMutex;
    uint32_t gVidcLogLevel;

    const char* KPILogPath = "/sys/kernel/boot_kpi/kpi_values";
    const char* KernelMsgPath = "/dev/kmsg";
    const char* LogLocalOutputDir = "/vendor_early_services/run/early_video_app";
    const char* LogLocalOutputPath = "/vendor_early_services/run/early_video_app/early_video_app.log";
    const int OneTimeLogCacheBufferSize = 256 * 1;
    char OneTimeLogCacheBuffer[OneTimeLogCacheBufferSize];

    void vidcUpdateLogLevel() {
        char debugLevel[PROPERTY_VALUE_MAX] = { 0 };
        // default value(0x3): VIDC_MSGLEVEL_ERROR | VIDC_MSGLEVEL_HIGH
        #ifndef _LINUX_VENV_
        property_get(kDebugLogsLevelProperty, debugLevel, "0x3");
        #endif
        gVidcLogLevel = static_cast<uint32_t>(strtoul(debugLevel, nullptr, 16));
        #ifdef _LINUX_VENV_
        gVidcLogLevel = 0x3;
        #endif
        gVidcLogLevel = VIDC_MSGLEVEL_HIGH;
    }

    void VIDC_ERR(const char* format, ...) {
        if (gVidcLogLevel >= VIDC_MSGLEVEL_ERROR) {
            va_list args, argsCopy1, argsCopy2;
            va_start(args, format);
            va_copy(argsCopy1, args);
            va_copy(argsCopy2, args);
            if (gVidcLogLevel >= VIDC_MSGLEVEL_MED) {
                printLogToLocalInternal(format, argsCopy1);
            }
            printLogToKMsgInternal(format, argsCopy2);
            va_end(args);
            va_end(argsCopy1);
            va_end(argsCopy2);
        }
    }

    void VIDC_HIGH(const char* format, ...) {
        if (gVidcLogLevel >= VIDC_MSGLEVEL_HIGH) {
            va_list args, argsCopy1, argsCopy2;
            va_start(args, format);
            va_copy(argsCopy1, args);
            va_copy(argsCopy2, args);
            if (gVidcLogLevel >= VIDC_MSGLEVEL_MED) {
                printLogToLocalInternal(format, argsCopy1);
            }
            printLogToKMsgInternal(format, argsCopy2);
            va_end(args);
            va_end(argsCopy1);
            va_end(argsCopy2);
        }
    }

    void VIDC_MED(const char* format, ...) {
        if (gVidcLogLevel >= VIDC_MSGLEVEL_MED) {
            va_list args, argsCopy1, argsCopy2;
            va_start(args, format);
            va_copy(argsCopy1, args);
            va_copy(argsCopy2, args);
            printLogToLocalInternal(format, argsCopy1);
            printLogToKMsgInternal(format, argsCopy2);
            va_end(args);
            va_end(argsCopy1);
            va_end(argsCopy2);
        }
    }

    void VIDC_LOW(const char* format, ...) {
        if (gVidcLogLevel >= VIDC_MSGLEVEL_LOW) {
            va_list args, argsCopy1, argsCopy2;
            va_start(args, format);
            va_copy(argsCopy1, args);
            va_copy(argsCopy2, args);
            printLogToLocalInternal(format, argsCopy1);
            printLogToKMsgInternal(format, argsCopy2);
            va_end(args);
            va_end(argsCopy1);
            va_end(argsCopy2);
        }
    }

    void printKPILog(const char* format, ...) {
        int KPILogFD = open(KPILogPath, O_WRONLY);
        if (KPILogFD > 0) {
            va_list args;
            va_start(args, format);
            char KPILogCacheBuffer[OneTimeLogCacheBufferSize];
            int length = vsnprintf(KPILogCacheBuffer, OneTimeLogCacheBufferSize, format, args);
            va_end(args);
            write(KPILogFD, KPILogCacheBuffer, length);
            close(KPILogFD);
        }
    }

    void printLogToKMsg(const char* format, ...) {
        if (format == NULL || *format == '\0') {
            return;
        }
        va_list args;
        va_start(args, format);
        printLogToKMsgInternal(format, args);
        va_end(args);
    }

    void printLogToLocal(const char* format, ...) {
        if (format == NULL || *format == '\0') {
            return;
        }
        va_list args;
        va_start(args, format);
        printLogToLocalInternal(format, args);
        va_end(args);
    }

    void closeLogInstance() {
        if (mInnerLogFile != NULL) {
            fclose(mInnerLogFile);
            mInnerLogFile = NULL;
        }
    }

    void printLogToKMsgInternal(const char* logFormat, va_list args) {
        if (logFormat == NULL || *logFormat == '\0') {
            return;
        }

        // First format using vsnprintf
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), logFormat, args);

        mLogMutex.lock();
        // Then output to kernel log
        freopen(KernelMsgPath, "w", stdout);
        std::cout << LogAPPTag << buffer << std::endl;
        mLogMutex.unlock();
    }

    bool createDirectoryRecursive(const char* path) {
        char tmp[256];
        char* p = NULL;

        // Copy input path to temporary buffer
        snprintf(tmp, sizeof(tmp), "%s", path);

        // Create directories level by level
        // Start from the second character (skip leading '/'), find path separators
        for (p = tmp + 1; *p; p++) {
            if (*p == '/') {
                // Found a path separator, temporarily terminate the current level path
                *p = '\0';
                
                // Create the current level directory
                // If creation fails and error is not "directory already exists", return false
                if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
                    return false;
                }
                
                // Restore the path separator and continue to next level
                *p = '/';
            }
        }

        // Create the final level directory
        if (mkdir(tmp, 0755) != 0 && errno != EEXIST) {
            return false;
        }

        return true;
    }

    void printLogToLocalInternal(const char* logFormat, va_list args) {
        if (logFormat == NULL) {
            return;
        }

        mLogMutex.lock();

        // print log to local file in case of stdout not flush to disk in time.
        if (mInnerLogFile == NULL) {

            const char* logCacheDir = LogLocalOutputDir;
            
            // Create log directory if it doesn't exist
            if (createDirectoryRecursive(logCacheDir)) {
                mInnerLogFile = fopen(LogLocalOutputPath, "w+b");
                if (mInnerLogFile == NULL) {
                }
            }
        }
        
        if (mInnerLogFile != NULL) {
            int length = vsnprintf(OneTimeLogCacheBuffer, OneTimeLogCacheBufferSize, logFormat, args);
            if (length > 0) {
                int wroteLength = fwrite(OneTimeLogCacheBuffer, sizeof(char), length, mInnerLogFile);
                int result = fflush(mInnerLogFile);
            }
        }

        mLogMutex.unlock();
    }

} // namespace early_video_app
