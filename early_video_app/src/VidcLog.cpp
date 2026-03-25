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
#include <format>
#include <cstdio>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <mutex>
#include <android-base/logging.h>

#include "../inc/VidcLog.h"

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
        mLogMutex.lock();
        freopen(KernelMsgPath, "w", stdout);

        std::cout << LogAPPTag;
        int i = 0;
        while (logFormat[i] != '\0') {
            if (logFormat[i] == '%') {
                i++;
                switch (logFormat[i]) {
                    case 's':
                        std::cout << va_arg(args, const char*);
                        break;
                    case 'd':
                        std::cout << va_arg(args, int);
                        break;
                    case 'l': {
                        if (logFormat[i + 1] == 'd') {
                            i++;
                            std::cout << va_arg(args, long);
                        }
                        else {
                            std::cout << '%' << logFormat[i];
                        }
                    }
                    case 'u':
                        std::cout << va_arg(args, unsigned int);
                        break;
                    case 'c':
                        std::cout << static_cast<char>(va_arg(args, long));
                        break;
                    case 'f':
                        std::cout << va_arg(args, double);
                        break;
                    case 'x':
                        std::cout << std::hex << va_arg(args, int) << std::dec;
                        break;
                    case '#': {
                        if (logFormat[i + 1] == 'x') {
                            i++;
                            unsigned int val = va_arg(args, unsigned int);
                            std::cout << std::hex << va_arg(args, unsigned int) << std::dec;
                        }
                        else {
                            std::cout << '%' << logFormat[i];
                        }
                        break;
                    }
                    case 'p':
                        std::cout << va_arg(args, long);
                        break;
                    default:
                        std::cout << '%' << logFormat[i];
                        break;
                }
            }
            else {
                std::cout << logFormat[i];
            }
            i++;
        }
        std::cout << std::endl;
        mLogMutex.unlock();
    }

    void printLogToLocalInternal(const char* logFormat, va_list args) {
        if (logFormat == NULL) {
            return;
        }

        mLogMutex.lock();

        //print log to local file in case of stdout not flush to disk in time.
        if (mInnerLogFile == NULL) {
            std::filesystem::path logCacheDir = LogLocalOutputDir;
            bool fileCheckResult = true;
            if (!std::filesystem::exists(logCacheDir)) {
                fileCheckResult = std::filesystem::create_directories(logCacheDir);
            }
            if (fileCheckResult) {
                mInnerLogFile = fopen(LogLocalOutputPath, "w+b");
            }
        }
        if (mInnerLogFile != NULL) {
            int length = vsnprintf(OneTimeLogCacheBuffer, OneTimeLogCacheBufferSize, logFormat, args);
            int wroteLength = fwrite(OneTimeLogCacheBuffer, sizeof(char), length, mInnerLogFile);
            int result = fflush(mInnerLogFile);
        }

        mLogMutex.unlock();
    }

} // namespace early_video_app
