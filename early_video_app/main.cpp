/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include "GTDecoder.h"
#include "VidcLog.h"

using namespace early_video_app;

int main(int argc, char **argv) {
    vidcUpdateLogLevel();
    VIDC_HIGH("main, enter\n");
    printKPILog("%s%s%s", LogKPITag, LogAPPTag, "run decoder");
    VIDC_HIGH("main, run decoder\n");
    int result = GTDecoder::runDecoder();
    if (result != 0) {
        printKPILog("%s%s%s", LogKPITag, LogAPPTag, "run decoder failed");
        VIDC_ERR("main: run decoder failed, result = %d\n", result);
    }
    printKPILog("%s%s%s", LogKPITag, LogAPPTag, "app exit");
    VIDC_HIGH("main, exit\n");
    closeLogInstance();

    return result;
}
