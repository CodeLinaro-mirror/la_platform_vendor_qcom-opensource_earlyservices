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
    const char* optstr = "i:o:";
    int c;
    extern int opterr;
    opterr = 0;
    vidcUpdateLogLevel();
    VIDC_HIGH("main, enter\n");

    while (1) {
        int option_index = 0;
        static struct option long_options[] =
        {
            {nullptr, 0, nullptr, 0}
        };
        c = getopt_long(argc, argv, "i:o:", long_options, &option_index);
        if (c == -1) {
            break;
        }

        switch (c) {
            case 0:
                break;
            case 'i':
                break;
            case 'o':
                break;
            default:
                break;
        }
    }

    VIDC_HIGH("main, run decoder\n");
    int result = GTDecoder::runDecoder();
    if (result != 0) {
        VIDC_ERR("main: run decoder failed, result = %d\n", result);
    }
    VIDC_HIGH("main, exit\n");
    closeLogInstance();

    return result;
}
