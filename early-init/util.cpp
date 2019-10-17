/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Not a contribution.
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include <android-base/file.h>
#include <android-base/strings.h>

#if defined(__ANDROID__)
#include <android-base/properties.h>

#else
#include "host_init_stubs.h"
#endif

#ifdef _INIT_INIT_H
#error "Do not include init.h in files used by ueventd or watchdogd; it will expose init's globals"
#endif

using namespace std::literals::string_literals;

namespace android {
namespace earlyinit {

void import_kernel_cmdline(bool in_qemu,
                           const std::function<void(const std::string&, const std::string&, bool)>& fn) {
    std::string cmdline;
    android::base::ReadFileToString("/proc/cmdline", &cmdline);
    for (const auto& entry : android::base::Split(android::base::Trim(cmdline), " ")) {
        std::vector<std::string> pieces = android::base::Split(entry, "=");
        if (pieces.size() == 2) {
            fn(pieces[0], pieces[1], in_qemu);
        }
    }
}
}  // namespace earlyinit
}  // namespace android
