/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include "Utils.h"
#include "VidcLog.h"

#include <dirent.h>
#include <string.h>
#include <unistd.h>

#if __has_include(<sys/sysmacros.h>)
#include <sys/sysmacros.h>   // Prefer system-provided major()/minor() macros
#endif

using namespace early_video_app;

void scanDevDirectory(const std::string& devPath) {
    VIDC_MED("Utils::scanDevDirectory, devPath = %s\n", devPath.c_str());

    DIR* dir = opendir(devPath.c_str());
    if (!dir) {
        VIDC_ERR("Utils::scanDevDirectory, opendir(%s) failed: %d (%s)\n",
                 devPath.c_str(), errno, strerror(errno));
        return;
    }

    struct dirent* de = nullptr;
    while ((de = readdir(dir)) != nullptr) {
        if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
            continue;
        }

        std::string fullPath = devPath;
        if (!fullPath.empty() && fullPath.back() != '/') fullPath += "/";
        fullPath += de->d_name;

        struct stat statbuf;
        // Use lstat(): do not follow symlinks. If you want to follow symlinks, use stat() instead.
        if (lstat(fullPath.c_str(), &statbuf) != 0) {
            VIDC_ERR("Utils::scanDevDirectory, lstat(%s) failed: %d (%s)\n",
                     fullPath.c_str(), errno, strerror(errno));
            continue;
        }

        if (S_ISCHR(statbuf.st_mode)) {
            VIDC_MED("Utils::scanDevDirectory, device: %s, Major: %d, Minor: %d, Permissions: %d\n",
                     de->d_name,
                     (int)major(statbuf.st_rdev),
                     (int)minor(statbuf.st_rdev),
                     (int)(statbuf.st_mode & 0777));
        }
    }

    closedir(dir);
}

