/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#ifndef __MSM_UTILS_H__
#define __MSM_UTILS_H__

#include <iostream>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>
#include <iomanip>
#include <algorithm>

void scanDevDirectory(const std::string& devPath);

#endif
