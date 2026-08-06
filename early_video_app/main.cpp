/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include <getopt.h>
#include <unistd.h>

#include "inc/GTDecoder.h"
#include "inc/VidcLog.h"

using namespace early_video_app;

int main(int argc, char **argv) {
    const char* shortOptions = "i:c:d:t:l:";
    struct option longOptions[] = {
        {"--input", required_argument, nullptr, 'i'},
        {"--config", required_argument, nullptr, 'c'},
        {"--displaycard", required_argument, nullptr, 'd'},
        {"--logtag", required_argument, nullptr, 't'},
        {"--loglevel", required_argument, nullptr, 'l'},
        {nullptr, 0, nullptr, 0}
    };
    char* sourceFilePath = nullptr;
    char* sourceConfigFilePath = nullptr;
    char* displayCard = nullptr;
    char* customLogTag = nullptr;
    int customLogLevel = VIDC_MSGLEVEL_ERROR | VIDC_MSGLEVEL_HIGH;
    while (true) {
        int optionIndex = 0;
        int c = getopt_long(argc, argv, shortOptions, longOptions, &optionIndex);
        if (c == -1) {
            break;
        }

        switch (c) {
            case 'i': {
                sourceFilePath = optarg;
                break;
            }
            case 'c': {
                sourceConfigFilePath = optarg;
                break;
            }
            case 'd': {
                displayCard = optarg;
                break;
            }
            case 't': {
                customLogTag = optarg;
                break;
            }
            case 'l': {
                char* end;
                customLogLevel = strtol(optarg, &end, 10);
                break;
            }
            default: {
                break;
            }
        }
    }
    vidcUpdateLogTag(customLogTag);
    vidcUpdateLogLevel(customLogLevel);
    VIDC_HIGH("main, enter\n");
    printKPILog("%s%s%s", LogKPITag, LogAPPTag, "app start");
    VIDC_MED("main, run decoder\n");
    int result = GTDecoder::runDecoder(sourceFilePath, sourceConfigFilePath, displayCard);
    if (result != 0) {
        printKPILog("%s%s%s", LogKPITag, LogAPPTag, "run decoder failed");
        VIDC_ERR("main: run decoder failed, result = %d\n", result);
    }
    printKPILog("%s%s%s", LogKPITag, LogAPPTag, "app exit");
    VIDC_HIGH("main, exit\n");
    closeLogInstance();

    return result;
}
