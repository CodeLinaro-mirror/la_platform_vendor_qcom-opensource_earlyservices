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

#ifndef _EARLYINIT_UTIL_H_
#define _EARLYINIT_UTIL_H_

#include <string>

namespace android {
namespace earlyinit {

void import_kernel_cmdline(bool in_qemu,
                           const std::function<void(const std::string&, const std::string&, bool)>&);
bool load_kernel_modules(int& loaded_count);

}  // namespace earlyinit
}  // namespace android

#endif
