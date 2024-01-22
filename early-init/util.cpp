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
/*
 * Changes from Qualcomm Innovation Center are provided under the following license:
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <thread>

#include <android-base/file.h>
#include <android-base/strings.h>
#include <android-base/logging.h>
#include <modprobe/modprobe.h>

#if defined(__ANDROID__)
#include <android-base/properties.h>

#else
#include "host_init_stubs.h"
#endif
#ifdef _INIT_INIT_H
#error "Do not include init.h in files used by ueventd or watchdogd; it will expose init's globals"
#endif

#define MODULES_DIR "/lib/modules"
#define MODULES_LOAD_FILE "modules.load"

using namespace std::literals::string_literals;

namespace android {
namespace earlyinit {

Modprobe _modprobe({MODULES_DIR}, MODULES_LOAD_FILE);

void import_kernel_cmdline(bool in_qemu,
                           const std::function<bool(const std::string&, const std::string&, bool)>& fn) {
    std::string cmdline;
    android::base::ReadFileToString("/proc/cmdline", &cmdline);
    for (const auto& entry : android::base::Split(android::base::Trim(cmdline), " ")) {
        std::vector<std::string> pieces = android::base::Split(entry, "=");
        if (pieces.size() == 2) {
            if (fn(pieces[0], pieces[1], in_qemu)) break;
        }
    }
}

bool load_kernel_modules(int& loaded_count, bool is_parallel) {
    Modprobe m({MODULES_DIR}, MODULES_LOAD_FILE);
    bool ret = (is_parallel) ? m.LoadModulesParallel(std::thread::hardware_concurrency())
                   : m.LoadListedModules(false);
    loaded_count = m.GetModuleCount();
    if (loaded_count > 0) {
        return ret;
    }

    return true;
}

bool insert_kernel_module(const std::string& mod) {
    return _modprobe.LoadWithAliases(mod, true);
}

static void kcmd_get_mparams(std::unordered_map<std::string, std::string> &opt) {
    constexpr int SZ = 2048;
    static char buf[SZ];
    char *mname, *op = 0, *val = 0;
    int i;
    bool quotes = false;
    {
        std::string cmdline;
        android::base::ReadFileToString("/proc/cmdline", &cmdline);
        strlcpy(buf, cmdline.c_str(), SZ-2);
        buf[SZ-2] = 0;
    }
    mname = &buf[0];
    auto addopt = [&] {
        if (mname && val) {
            // Add param to map
            std::string modn = mname;
            std::replace(modn.begin(), modn.end(), '-', '_');
            auto iter = opt.find(modn);
            std::string opts = op;
            opts += "=";
            opts += val;
            if (iter != opt.end())
                iter->second = iter->second + " " + opts;
            else
                opt.emplace(modn, opts);;
        }
    };

    for (i = 0; buf[i] != 0; i++) {
        if (buf[i] == '"') quotes = !quotes;
        if (quotes) continue;

        if (buf[i] == ' ') {
            if (val) {
                buf[i] = 0;
                addopt();
            }
            mname = &buf[i+1];
            op = 0;
            val = 0;
            continue;
        }
        if (buf[i] == '.') {
            if (op == 0) {
                buf[i] = 0;
                op = &buf[i+1];
            }
            continue;
        }
        if (buf[i] == '=') {
            if (op) {
                buf[i] = 0;
                val = &buf[i+1];
            }
            continue;
        }
    }
    if (val && !quotes) {
        addopt();
    }
}

int get_kernel_module_param(const std::string &mod_name, std::string& params,
    std::unordered_map<std::string, std::string>& opt, bool init) {

    if (init) {
        kcmd_get_mparams(opt);
        return 0;
    }

    std::string mname = mod_name;
    // - and _ are considered same in mod name
    std::replace(mname.begin(), mname.end(), '-', '_');
    params = "";
    auto iter = opt.find(mname);
    if (iter != opt.end()) {
        params = iter->second;
    }

    return 0;
}

void import_kernel_bootconfig(bool in_qemu,
                           const std::function<bool(const std::string&, const std::string&, bool)>& fn) {
    std::string bootconfig;
    android::base::ReadFileToString("/proc/bootconfig", &bootconfig);
    for (const auto& entry : android::base::Split(bootconfig, "\n")) {
      std::vector<std::string> pieces = android::base::Split(entry, "=");
      if (pieces.size() == 2) {
        if (fn(android::base::Trim(pieces[0]), android::base::Trim(pieces[1]), in_qemu)) {
          break;
        }
      }
    }
}

}  // namespace earlyinit
}  // namespace android
