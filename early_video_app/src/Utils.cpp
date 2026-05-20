/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include "../inc/Utils.h"
#include "../inc/VidcLog.h"

#ifndef major
#define major(dev) ((dev) >> 8)
#endif
#ifndef minor
#define minor(dev) ((dev) & 0xff)
#endif

using namespace early_video_app;

void scanDevDirectory(const std::string& devPath) {
    try {
		VIDC_MED("Utils::scanDevDirectory, devPath = %s\n", devPath.c_str());
        for (const auto& entry : std::filesystem::directory_iterator(devPath)) {
            if (entry.is_character_file()) {
                const std::string filename = entry.path().filename().string();
				struct stat statbuf;
				if (stat(devPath.c_str(), &statbuf) == 0) {
                    VIDC_MED("Utils::scanDevDirectory, device: %s, Major: %d, Minor: %d, Permissions: %d\n",
                        filename.c_str(), major(statbuf.st_rdev), minor(statbuf.st_rdev), statbuf.st_mode & 0777);
				}
            }
        }
    }
    catch (const std::filesystem::filesystem_error& ex) {
        VIDC_ERR("Utils::scanDevDirectory, Error accessing %s : %d\n", devPath.c_str(), ex.what());
    }
}

