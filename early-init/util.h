/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Not a contribution.
 *
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef _EARLYINIT_UTIL_H_
#define _EARLYINIT_UTIL_H_

#include <string>
#include <unordered_map>

namespace android {
namespace earlyinit {

void import_kernel_cmdline(bool in_qemu,
                           const std::function<bool(const std::string&, const std::string&, bool)>&);
bool load_kernel_modules(int& loaded_count, bool is_parallel);
bool insert_kernel_module(const std::string& mod);

int get_kernel_module_param(const std::string &mod_name, std::string& params,
     std::unordered_map<std::string, std::string>& opt, bool init = false);
void import_kernel_bootconfig(bool in_qemu,
                           const std::function<bool(const std::string&, const std::string&, bool)>&);
}  // namespace earlyinit
}  // namespace android

#endif
