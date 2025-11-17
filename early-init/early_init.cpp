/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
* Changes from Qualcomm Technologies, Inc. are provided under the following license:
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted (subject to the limitations in the
* disclaimer below) provided that the following conditions are met:
*
*     * Redistributions of source code must retain the above copyright
*       notice, this list of conditions and the following disclaimer.
*
*     * Redistributions in binary form must reproduce the above
*       copyright notice, this list of conditions and the following
*       disclaimer in the documentation and/or other materials provided
*       with the distribution.
*
*     * Neither the name of Qualcomm Innovation Center, Inc. nor the names of its
*       contributors may be used to endorse or promote products derived
*       from this software without specific prior written permission.
*
* NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
* GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
* HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
* IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
* GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
* IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
* IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
//#define TEMP_SOLUTION
//#define EARLYINIT_DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <pwd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sched.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <sys/un.h>
#include <android-base/file.h>
#include <cutils/android_filesystem_config.h>
#include <sys/sysinfo.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <utils/Log.h>

// for file copy
#include <filesystem>

#ifdef EARLYINIT_DEBUG
#include <dirent.h>
#endif

#define DEFAULT_CONF            "/vendor_early_services/etc/early_init.conf"
#define ANDROID_U_CONF          "/vendor_early_services/etc/early_init_u.conf"
#define ES_GEN4_CONF            "/vendor_early_services/etc/early_init_gen4.conf"
#define END_TAG                 "<end>"
#define LINE_MAX                2048
#define SHORT_STRING_MAX        128
#define VS_STRING_MAX           32
#define WHITESPACE              " \t\n\r"
#define KPI_VALUE_PATH          "/sys/kernel/boot_kpi/kpi_values"
#define GPIO_EXPORT             "/sys/class/gpio/export"
#define AUDIO_PKT               "/dev/aud_pasthru_adsp"
#define MSM_ION                 "/dev/msm_audio_ion"
#define MSM_ION_CMA             "/dev/msm_audio_ion_cma"
#define MSM_SYSTEM              "/dev/dma_heap/system"
#define MSM_AUDIO_ML            "/dev/dma_heap/qcom,audio-ml"
#define VIDEO32_DEVICE_PATH     "/dev/video32"
#define VIDEO33_DEVICE_PATH     "/dev/video33"
#define AUDIO_FW_PATH           "/vendor_early_services/vendor/firmware_mnt"
#define AUDIO_ADSP_FW_PATH      "vendor_early_services/vendor/firmware_mnt/image/adsp.mdt"
#define LXC_ROOTFS_PATH         "/vendor_early_services/vendor/vm-system"
#define PCM_ID_PATH             "/proc/asound/card0/id"
#define MSM_AUDIO_ION_PATH      "/sys/class/msm_audio_ion/msm_audio_ion/uevent"
#define MSM_AUDIO_ION_CMA_PATH  "/sys/class/msm_audio_ion_cma/msm_audio_ion_cma/uevent"
#define AUDIO_PKT_PATH   "/sys/class/aud_pasthru_adsp/aud_pasthru_adsp/uevent"
#define MSM_SYSTEM_PATH         "/sys/class/dma_heap/system/uevent"
#define MSM_AUDIO_ML_PATH       "/sys/class/dma_heap/qcom,audio-ml/uevent"
#define PCM_PATH                "/proc/asound/pcm"
#define MSM_SOUND_CTRL_PATH     "/sys/class/sound/controlC0/uevent"
#define SMACK_LABEL_PATH        "/proc/self/attr/current"
#define SMACK_LABEL             "System"
#define DEFAULT_PATH            "/sbin:/usr/sbin:/bin:/usr/bin:/system/sbin:/system/bin:/system/xbin:/odm/bin:/vendor/bin:/vendor/xbin:vendor_early_services/sbin:vendor_early_services/system/sbin:vendor_early_services/system/bin:vendor_early_services/system/xbin:vendor_early_services/odm/bin:vendor_early_services/vendor/bin:vendor_early_services/vendor/xbin"

#define SND_CARD_DIR "/dev/snd"
#define EARLY_SERVICES_SEPOL   "/vendor_early_services/vendor/etc/selinux/precompiled_sepolicy"

#define STR_EXPAND(tok) #tok
#define TO_STRING(tok) STR_EXPAND(tok)

#include "util.h"
#include <sys/sysmacros.h>
#include <log.h>

#include <android-base/chrono_utils.h>
#include <android-base/file.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include <selinux/android.h>
#include <android-base/unique_fd.h>
#include <unistd.h>
#include <optional>
#include <android-base/logging.h>
#include "log.h"
#include <selinux/selinux.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

using android::base::boot_clock;

#define init_module(module_image, len, param_values) syscall(__NR_init_module, module_image, len, param_values)
#define finit_module(fd, param_values, flags) syscall(__NR_finit_module, fd, param_values, flags)
#define ADSP_LOADER_KO          "adsp_loader_dlkm"
//#define DISP_DRM_DPU0_READY_PATH     "/sys/devices/platform/soc/ae00000.qcom,mdss_mdp/init_complete"
#define DISP_DRM_DRIVER_CARD3_READY_PATH  "/sys/class/drm/card3/uevent"
#define DISP_DRM_DRIVER_CARD4_READY_PATH  "/sys/class/drm/card4/uevent"
#define DISP_DRM_DRIVER_RENDER_READY_PATH  "/sys/class/drm/renderD128/uevent"

#ifdef PLATFORM_GEN4
#define DISP_DRM_DPU1_READY_PATH     "/sys/devices/platform/soc/22000000.qcom,mdss_mdp/init_complete"
#endif

#define AUDIO_CTRL_PATH         "/dev/snd/pcmC0D50p"
#define CAMERA_MDEV_PATH        "/dev/media0"
#define CAMERA_VDEV_PATH        "/dev/video0"
#define CAMERA_V4L_DEV_PATH     "/dev/v4l-subdev0"
#define DMA_HEAP_DIR            "/dev/dma_heap"
#define CAMERA_DMA_HEAP_PATH    "/dev/dma_heap/qcom,system"

#define VIDEO_SYS_DMA_HEAP_PATH "/dev/dma_heap/qcom,system"

#define ES_FW_CHK_PATH          "/vendor_early_services/vendor/firmware_mnt/image"
#define ES_KMOD_DONE            "/dev/kmdone"

#define ECHIME_APP              "early_chime"
#define EAUDIO_APP              "early_audio"
#define PDMAPPER_APP            "pd-mapper"
#define ELXC_APP                "init_early_lxc"
#define EMOD_END                "mod_end"

#define WAIT_SET_PERM_SECS  15
#ifdef PLATFORM_GEN4
#define WAIT_SET_PERM_MSECS 1000
#else
#define WAIT_SET_PERM_MSECS 400
#endif
#define WAIT_EAPP_SECS      20
#define WAIT_EAPP_MSECS     2500
#define WAIT_SLEEP_MSEC     5
#define WAIT_SLEEP_USECS    500
#define WAIT_PID_MIN_MSECS  2000
#define WAIT_PID_MAX_MSECS  5000
#define EAPPS_MAX           12

#define ES_DFLMOD_PATH   "/lib/modules/"
#define ES_VMOD_PATH     "/vendor_early_services/vendor/lib/modules/"

#define EMOD_DEF_TAG_1   "def_1"
#define EMOD_DEF_TAG_2   "def_2"
#define EMOD_DI_TAG      "display"

#if defined(PLATFORM_GEN4)
#define ES_DFLMOD_ORDER_1     ES_VMOD_PATH"modules_gen4.order"
#define ES_DFLMOD_ORDER_DI    ES_VMOD_PATH"modules_di_gen4.order"
#else
#define ES_DFLMOD_ORDER_1     ES_VMOD_PATH"modules.order"
#define ES_DFLMOD_ORDER_DI    ES_VMOD_PATH"modules_di.order"
#endif
#define ES_DFLMOD_ORDER_2     ES_VMOD_PATH"modules_2.order"

#define EAPP_WAIT_DEFAULT 0x00
#define EAPP_WAIT_NONE   0x01
#define EAPP_WAIT_DISP   0x02
#define EAPP_MOD_WAIT_FW 0x04

#define LMP_MODPROBE       0
#define LMP_DIRECT         1
#define LMP_DIRECT_CHK_AUD 3

#define ES_CTYPE_FW         1
#define ES_CTYPE_DEF2_MOD   2
#define ES_CTYPE_DI_MOD     3
#define ES_CTYPE_LOAD_SE    4

#define PIPE_RD 0
#define PIPE_WR 1

#if defined(__ANDROID_U__) || defined(PLATFORM_GEN4)
#define SELINUXMNT "/sys/fs/selinux"

#define TEST_APP "init_early_test"
#define TEST_APP_CMD  "/vendor_early_services/system/bin/init_early_test"
#define TEST_APP_ENV "/vendor_early_services:/vendor_early_services/system:/vendor_early_services/system/lib64:/vendor_early_services/system/bin/bootstrap"
#define TEST_APP_PID "/vendor_early_services/run/early/init_early_test.pid"
#define TEST_APP_LOG "/vendor_early_services/run/init_early_test.txt"
#endif //__ANDROID_U__ || PLATFORM_GEN4

#ifdef EARLYINIT_DEBUG
static inline bool is_empty_line(const char* p);
static inline char *strstrip(char *s);
#endif
static inline int parse_line(char* p);
static void set_permissions(const char *path, int permissions, int user, int group, const char *context);
static void __attribute__((unused)) launch_early_apps(void);
static void set_video_permission(void);
static void set_camera_media_permission(void);
static void set_camera_video_permission(void);
static void set_camera_v4l_permission(void);
static int check_audio_ar_ready(void);
#ifdef ES_AUDIOE_DISABLED
static void set_audio_permission(void);
static int check_audio_device_ready(void);
#endif
static int load_kmod_and_nodes(const char* mod_group);
static int wait_for_file(const char* file, int sleep_msec, int count, bool log_fail = false);
static int check_video_device_ready(void);
static int check_ais_device_ready(void);
static int check_rvc_device_ready(void);
static int check_pdmapper_ready(void);
static int check_display_driver_ready(void);
static int check_lxc_device_ready(void);

enum EnforcingStatus { SELINUX_PERMISSIVE, SELINUX_ENFORCING };

char chipId[32]  = { 0 };
char platformId[32]  = { 0 };
// Global variables
bool _use_min_wait = true;
bool _audio_reach = false;
std::string _boot_slot;
std::unordered_map<std::string, std::string> _module_params;

static struct {
  char* appname;
  char* cmd;
  char* applog;
  char* pidfile;
  int   env_used;
  char* env[32];
  int   argv_used;
  char* argv[32];
  char* gpio;
  int   usleep;
  int   bindcpumask;
  int   priority;
  char* username;
  char* group;
  char* wait;
  char* selabel;
} app_launcher;

const static struct {
  char name[VS_STRING_MAX];
  char kfile[VS_STRING_MAX];
  char tag[VS_STRING_MAX];
  int (*is_ready)(void);
  int wait;
} _eapp_info[] = {
#ifdef PLATFORM_GEN4
 {"qcxserver", "modules_qcx.order", "qcx", check_ais_device_ready, EAPP_MOD_WAIT_FW},
 {"qcarcam_edrm_rvc", "modules_rv_gen4.order", "rvc", check_rvc_device_ready, EAPP_MOD_WAIT_FW},
 {EMOD_END, "modules_end_gen4.order", "def_end", NULL, EAPP_WAIT_NONE},
#else
 {"ais_server", "modules_ais.order", "ais", check_ais_device_ready, EAPP_MOD_WAIT_FW},
 {"qcarcam_edrm_rvc", "modules_rv.order", "rvc", check_rvc_device_ready, EAPP_WAIT_DISP},
 {EMOD_END, "modules_end.order", "def_end", NULL, EAPP_WAIT_NONE},
#endif //PLATFORM_GEN4
 {"esplash", "", "splash", NULL, EAPP_WAIT_DISP},
 {"earlyVideo", "modules_vi.order", "video", check_video_device_ready, EAPP_MOD_WAIT_FW},
 {"pd-mapper", "", "pd-mapper", check_pdmapper_ready, EAPP_MOD_WAIT_FW},
 {EAUDIO_APP, "modules_r_au.order", EAUDIO_APP, check_audio_ar_ready, EAPP_MOD_WAIT_FW},
 {"init_early_lxc", "", "init_early_lxc", check_lxc_device_ready, EAPP_WAIT_DISP},
#ifdef ES_AUDIOE_DISABLED
 {"", "modules_au.order", "audio", check_audio_device_ready, EAPP_MOD_WAIT_FW},
#endif
 {"", "", "", NULL, EAPP_WAIT_DEFAULT} // Last Entry
};
static pid_t _eapp_pid[EAPPS_MAX];
static const char EARLY_DFL_APP[] = "early_services";

#define BIT_SET(p,n) ((p) & (1 << (n)))
#define uid_is_valid(uid) ((uid != (uid_t) UINT32_C(0xFFFFFFFF)) && \
            (uid != (uid_t) UINT32_C(0xFFFF)))
#define gid_is_valid(gid)  uid_is_valid(gid)

static void create_drm_udev_cards(void);

enum drm_udev_cards {
#ifdef PLATFORM_GEN4
  card2 = 0,
  card3,
  card4,
  card5,
#else
  card2 = 0,
  renderD128,
  card3,
  card4,
#endif
  cards_max
};

#define FAST_RVC_CARD           card4

struct drm_cards_info {
  const char *sysfs_path;
  const char *num;
  const char *udev_dir;
  const char *udev_node_path;
  bool is_created;
} _drm_cards[] = {
#ifdef PLATFORM_GEN4
  {"/sys/class/drm/card2/uevent", "card2", "/dev/dri", "/dev/dri/card2", false},
  {"/sys/class/drm/card3/uevent", "card3", "/dev/dri", "/dev/dri/card3", false},
  {"/sys/class/drm/card4/uevent", "card4", "/dev/dri", "/dev/dri/card4", false},
  {"/sys/class/drm/card5/uevent", "card5", "/dev/dri", "/dev/dri/card5", false},
#else
  {"/sys/class/drm/card2/uevent", "card2", "/dev/dri", "/dev/dri/card2", false},
  {"/sys/class/drm/renderD128/uevent", "renderD128", "/dev/dri", "/dev/dri/renderD128", false},
  {"/sys/class/drm/card3/uevent", "card3", "/dev/dri", "/dev/dri/card3", false},
  {"/sys/class/drm/card4/uevent", "card4", "/dev/dri", "/dev/dri/card4", false},
#endif
};

static void inline safe_free(char** p)
{
  if (*p)
    free(*p);
  *p = NULL;
  return;
}

static void inline safe_close(int fd)
{
  if (fd > 0)
    close(fd);
  return;
}

static void inline write_marker(const char* name)
{
#ifdef __ANDROID_U__
  ALOGE("boot_kpi: %s ", name);
#else
  int fd = -1;

  fd = open(KPI_VALUE_PATH, O_WRONLY);
  if (fd > 0) {
    (void)write(fd, name, strlen(name));
  } else {
    LOG(INFO) << "Open bootkpi for name " << name << " failed, errno " << errno;
    printf("open bootkpi for name %s failed %s\r\n", name, strerror(errno));
  }
  safe_close(fd);
#endif
  return;
}

static void inline print_log(const char* str)
{
  if (str == NULL) return;
  freopen("/dev/kmsg", "w", stdout);
  printf("ES: %s \r\n", str);
}

#ifdef EARLYINIT_DEBUG
static void inline write_smack_label(char* label)
{
  int fd = -1;

  fd = open(SMACK_LABEL_PATH, O_WRONLY);
  if (fd > 0) {
    (void)write(fd, label, strlen(label));
  } else {
    printf("write label  %s failed %s\r\n", label, strerror(errno));
  }
  safe_close(fd);

  return;
}
#endif

/*
 * Only support abs path
 */
static inline void mkdirs(const char* p, mode_t mode)
{
  char str[1024] = {0};
  struct stat st;
  int i = 0, len = 0, ret = 0;

  len = strlen(p);
  if (len > 1024)
    printf("input string is too long\r\n");

  strlcpy(str, p, sizeof(str));

  if (len <= 0 || str[0] != '/')
    return;

  if (str[len - 1] == '/') {
    len--;
    str[len] = '\0';
  }

  for (i = 1; i < len; i++) {
    if (str[i] == '/') {
      str[i] = '\0';
      if (stat(str, &st) == -1) {
        ret = mkdir(str, 0755);
        if (ret < 0)
          perror("mkdir failed");
      }
      str[i] = '/';
    }
  }

  if (stat(str, &st) == -1) {
    ret = mkdir(str, mode);
    if (ret < 0)
      perror("mkdir failed");
  }

  return;
}

static inline void prepare_dir(char* p)
{
  struct stat st;
  int ret = 0;

  switch (*p) {
    case 'd':
      if (0 == strncmp(p + 1, "ebugfs", strlen("ebugfs"))) {
        /*
         * Mount debugfs
         */
        if (stat("/sys/kernel/debug", &st) == -1) {
          perror("/sys/kernel/debug folder doesn't exist");
          mkdirs("/sys/kernel/debug", 0755);
        }

        ret = mount("debugfs", "/sys/kernel/debug", "debugfs", 0, NULL);
        if (ret < 0) {
          perror("mount debugfs failed");
        }
      } else if(0 == strncmp(p + 1, "ev", strlen("ev"))) {
        if (stat("/vendor_early_services/dev", &st) == -1) {
          perror("/vendor_early_services/dev folder doesn't exist");
          mkdir("/vendor_early_services/dev", 0755);
        }
	ret = mount("devtmpfs", "/vendor_early_services/dev", "devtmpfs", 0, NULL);
        if (ret < 0) {
            freopen("/dev/kmsg", "w", stdout);
            printf(" /vendor_early_services/dev mount failed error = %d \n", errno);
          perror(" mount /vendor_early_services/dev with devtmpfs failed ");
        } else
            freopen("/dev/kmsg", "w", stdout);
            printf("/vendor_early_services/dev mount success error = %d \n", errno);
      }
      break;
    case 'x':
      if (0 == strncmp(p + 1, "dg_runtime_dir", strlen("dg_runtime_dir"))) {
        /*
         * Prepare dir for weston socket
         */
        if (stat("/run", &st) == -1) {
          perror("/run folder doesn't exist");
          ret = mkdir("/run", 0700);
          if (ret < 0)
            perror("mkdir failed");
        }

#ifdef LV_CODE
        ret = mount("tmpfs", "/run", "tmpfs", MS_NOSUID|MS_NODEV|MS_STRICTATIME, "mode=755,smackfsroot=*");
#endif
        ret = mount("tmpfs", "/run", "tmpfs", MS_NOSUID|MS_NODEV|MS_STRICTATIME, "mode=755");
        if (ret < 0) {
          perror("mount tmpfs failed");
        }
#ifdef LV_CODE
        mkdirs(DISPLAY_XDG_RUNTIME_DIR, 0775);

        struct passwd *pw;
        pw = getpwnam(TO_STRING(WESTON_USER));
        if (!pw) {
          perror("username is not exist\r\n");
        } else {
          (void)chown(DISPLAY_XDG_RUNTIME_DIR, pw->pw_uid, pw->pw_gid);
        }
#endif
        mkdirs("/run/early", 0775);
      }
      break;
    case 's':
      if (0 == strncmp(p + 1, "hm", strlen("hm"))) {
        mkdirs("/dev/shm", 0777);
        ret = mount("tmpfs", "/dev/shm", "tmpfs", 0, NULL);
        if (ret < 0) {
          perror("mount tmpfs failed");
        }
      } else if (0 == strncmp(p + 1, "ysfs", strlen("ysfs"))) {
        /*
         * Mount sysfs
         */
        if (stat("/sys", &st) == -1) {
          mkdir("/sys", 0755);
        }

        ret = mount("sysfs", "/sys", "sysfs", 0, NULL);
        if (ret < 0) {
          perror("mount sysfs failed");
        }
      } else if (0 == strncmp(p + 1, "elinuxfs", strlen("elinuxfs"))) {
        /*
         * Mount selinuxfs
         */
        ret = mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL);
        if (ret < 0) {
          perror("mount sysfs failed");
        } else {
          printf("selinuxfs is mounted \r\n");
        }
      } else {
        printf("warning unknown input string %s for prepare_dir", p);
      }
      break;
    case 'p':
      if (0 == strncmp(p + 1, "rocfs", strlen("rocfs"))) {
        if (stat("/proc", &st) == -1) {
          mkdir("/proc", 0755);
        }
        ret = mount("proc", "/proc", "proc", 0, NULL);
        if (ret < 0) {
          perror("mount procfs failed");
        }
      }
      break;
	case 'c':
      if (0 == strncmp(p + 1, "group2", strlen("group2"))) {
        if (stat("/sys/fs/cgroup", &st) == -1) {
          perror("/sys/fs/cgroup folder doesn't exist");
          mkdir("/sys/fs/cgroup", 0755);
        }
          ret = mount("none", "/sys/fs/cgroup", "cgroup2", 0, NULL);
          if (ret < 0) {
            freopen("/dev/kmsg", "w", stdout);
            printf(" /sys/fs/cgroup mount failed error = %d \n", errno);
            perror(" mount /sys/fs/cgroup with cgroup2 failed ");
          } else {
            freopen("/dev/kmsg", "w", stdout);
            printf("/sys/fs/cgroup mount success error = %d \n", errno);
          }
      }
      break;
    default:
      printf("warning unknown input string %s for prepare_dir", p);
  }
  return;
}

#ifdef EARLYINIT_DEBUG
/*
 * Remove trailing spaces
 */
static inline char *strstrip(char *s) {
  char* end = s + strlen(s) - 1;

  while (end > s) {
    if (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') {
      end--;
    } else {
      break;
    }

  }

  *(end+1) = 0;

  return s;
}
#endif

// enforce_user according to user settings
// if fail, fallback to root user
static void inline enforce_user(char* username)
{
  struct passwd *pw;

  pw = getpwnam(username);
  if (!pw) {
    perror("username is not exist\r\n");
    return;
  }

  printf("uid is %d", pw->pw_uid);
  if (!uid_is_valid(pw->pw_uid)) {
    perror("uid is not valid\r\n");
  }
  if (0 != setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid)) {
    perror("setresuid failed\r\n");
  }
}

// enforce_group according to group settings
// if fail, fallback to root group
static void inline enforce_group(char* group)
{
  struct passwd *pw;
  pw = getpwnam(group);
  if (!pw) {
    perror("group is not exist\r\n");
    return;
  }

  printf("gid is %d", pw->pw_gid);
  if (!gid_is_valid(pw->pw_gid)) {
    perror("gid is not valid\r\n");
  }
  if (0 != setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid)) {
    perror("setresgid failed\r\n");
  }
}

/*
 * Suceess, return 0, else return -1
 */
static int inline find_rvalue(char** p) {
  char *t;
  int ret = -1;
  int len;

  t = strchr(*p, '=');
  if (t)
    *p = t;
  else
    goto out;

  (*p)++;
  len = strlen(*p);

  while (**p == ' ' || **p == '\t')
  {
    (*p)++;
    len--;
    if (len == 0)
      break;
    printf("please remove redundant space.\r\n");
  }
  if (len > 0)
    ret = 0;

out:
  return ret;
}

static void inline app_launcher_start_over(void)
{
  int i = 0;

  safe_free(&app_launcher.appname);
  safe_free(&app_launcher.cmd);
  safe_free(&app_launcher.applog);
  safe_free(&app_launcher.gpio);
  safe_free(&app_launcher.pidfile);
  safe_free(&app_launcher.wait);
  safe_free(&app_launcher.username);
  safe_free(&app_launcher.group);
  safe_free(&app_launcher.selabel);
  app_launcher.usleep = -1;

  for (i = 0; i < app_launcher.argv_used; i++)
    safe_free(&app_launcher.argv[i]);

  for (i = 1; i < app_launcher.env_used; i++)
    safe_free(&app_launcher.env[i]);

  app_launcher.argv_used = 0;
  app_launcher.env_used = 0;
  app_launcher.bindcpumask = -1;
  app_launcher.priority = -1;
  app_launcher.env[app_launcher.env_used++] = (char*)DEFAULT_PATH; //set DEFAULT_PATH as static env[0] path for all ES app's

  return;
}

/*
 * Remove redundant whitespace
 */
static inline pid_t parse_line(char* p)
{
  size_t i = 0;
  char* t;
  pid_t pid = -1;
  int fd;
  int ret = 0;
  char pid_file[10] = {0};
  static char marker[50];

  /*
   * Skip whitespace and comment line
   */
  for (i = 0; i < strlen(p); i++) {

    if (p[i] == ' ' || p[i] == '\t')
      continue;

    if (p[i] == '#')
      goto out;
    else
      break;

    p += i;
  }

  switch (*p) {

    case '[':
      t = strchr(p, ']');
      if (t) {
        app_launcher_start_over();
        *t = '\0';
        p++;
        app_launcher.appname= strdup(p);
      }
      break;
    case 'c':/* cmd */
      if (0 == strncmp(p + 1, "md", strlen("md")) && 0 == find_rvalue(&p)) {
        app_launcher.cmd = strdup(p);
        app_launcher.argv[app_launcher.argv_used] = strdup(p);
        printf("argv[%d] is %s \r\n", app_launcher.argv_used, p);
        app_launcher.argv_used++;
      }
      break;
    case 'e':/* env */
      if (0 == strncmp(p + 1, "nv", strlen("nv")) && 0 == find_rvalue(&p) && app_launcher.env_used < 31) {
        app_launcher.env[app_launcher.env_used] = strdup(p);
        printf("env[%d] is %s \r\n", app_launcher.env_used, p);
        app_launcher.env_used++;
      }
      break;
    case 'a':/* argv */
      if (0 == strncmp(p + 1, "rgv", strlen("rgv")) && 0 == find_rvalue(&p) && app_launcher.argv_used < 31) {
        app_launcher.argv[app_launcher.argv_used] = strdup(p);
        printf("argv[%d] is %s \r\n", app_launcher.argv_used, p);
        app_launcher.argv_used++;
      }
      break;
    case 'l':/* applog */
      if (0 == strncmp(p + 1, "og", strlen("og")) && 0 == find_rvalue(&p)) {
        app_launcher.applog = strdup(p);
      }
      break;
    case 'g':/* gpio */
      if (0 == strncmp(p + 1, "pio", strlen("pio")) && 0 == find_rvalue(&p)) {
        app_launcher.gpio = strdup(p);
      }
      if (0 == strncmp(p + 1, "roup", strlen("roup")) && 0 == find_rvalue(&p)) {
        app_launcher.group = strdup(p);
      }
      break;
    case 'w':/* wait */
      if (0 == strncmp(p + 1, "ait", strlen("ait")) && 0 == find_rvalue(&p)) {
        app_launcher.wait = strdup(p);
      }
      break;
    case 'p':/* pidfile */
      if (0 == strncmp(p + 1, "idfile", strlen("idfile")) && 0 == find_rvalue(&p)) {
        app_launcher.pidfile = strdup(p);
      }
      if (0 == strncmp(p + 1, "riority", strlen("riority")) && 0 == find_rvalue(&p)) {
        app_launcher.priority = atoi(p);
        printf("priority is %d \r\n", app_launcher.priority);
      }
      break;
    case 'm':/* msleep */
      if (0 == strncmp(p + 1, "sleep", strlen("sleep")) && 0 == find_rvalue(&p)) {
        app_launcher.usleep = atoi(p) * 1000;
        printf("usleep is %d \r\n", app_launcher.usleep);
      }
      break;
    case 'b':/* bindcpumask */
      if (0 == strncmp(p + 1, "indcpumask", strlen("indcpumask")) && 0 == find_rvalue(&p)) {
        app_launcher.bindcpumask = atoi(p);
        if (app_launcher.bindcpumask < -1 || app_launcher.bindcpumask > 15)
          app_launcher.bindcpumask = -1;
	 printf("bindcpumask is %d", app_launcher.bindcpumask);
      }
      break;
    case 'u':
      if (0 == strncmp(p + 1, "ser", strlen("ser")) && 0 == find_rvalue(&p)) {
        app_launcher.username = strdup(p);
      }
      break;
    case 's':
      if (0 == strncmp(p + 1, "elabel", strlen("elabel")) && 0 == find_rvalue(&p)) {
        app_launcher.selabel = strdup(p);
      }
      break;
    case '<':/* end */
      /*
       * When comes to the end, start up the app
       */
      if (strncmp(p, END_TAG, strlen(END_TAG)))
        goto out;

      if ((!strncmp(app_launcher.appname, ECHIME_APP, strlen(ECHIME_APP))
             && _audio_reach) ||
          (!strncmp(app_launcher.appname, PDMAPPER_APP, strlen(PDMAPPER_APP))
             && !_audio_reach)) {
        LOG(INFO) << "ES : Not Launching app " << app_launcher.appname;
        goto out;
      }


      pid = clone(nullptr, nullptr, (CLONE_FS | SIGCHLD), nullptr);

      if (pid < 0) {
        LOG(INFO) << " early_init fork child process failed ";
        perror("fork child process failed \r\n");
        goto out;
      }

      if (0 == pid) {
        /*
         * Handle log redirect
         */
        if (app_launcher.applog) {
          fd = open(app_launcher.applog, O_RDWR | O_CREAT, 0666);
          if (fd > 0) {
            dup2(fd, fileno(stdout));
            dup2(fd, fileno(stderr));
            safe_close(fd);
            safe_close(fd);
          }
        } else {
          fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
          dup2(fd, STDOUT_FILENO);
          dup2(fd, STDERR_FILENO);
          close(fd);
        }

        if (app_launcher.bindcpumask != -1) {
          cpu_set_t mask;
          CPU_ZERO(&mask);
          for (int i = 0; i < get_nprocs_conf(); i++) {
            if (BIT_SET(app_launcher.bindcpumask, i))
              CPU_SET(i, &mask);
          }
          if (0 != sched_setaffinity(0, sizeof(mask), &mask))
            printf("sched_setaffinity failed %d %s\r\n", app_launcher.bindcpumask, strerror(errno));
        }

        if (app_launcher.priority > 0) {
          struct sched_param sp;
          memset( &sp, 0, sizeof(sp) );
          sp.sched_priority = app_launcher.priority;
          if (0 != sched_setscheduler( pid, SCHED_FIFO, &sp))
            printf("sched_setparam failed %d %s\r\n", app_launcher.priority, strerror(errno));
        }

        if (app_launcher.gpio) {
          fd = open(GPIO_EXPORT, O_WRONLY);
          if (fd < 0)
            perror("open gpio export node failed \r\n");
          else {
            if (-1 == write(fd, app_launcher.gpio,strlen(app_launcher.gpio)))
              printf("config gpio to %s failed: %s", app_launcher.gpio, strerror(errno));
          }
          safe_close(fd);
        }

        if (app_launcher.pidfile) {
          fd = open(app_launcher.pidfile, O_WRONLY | O_CREAT, 0666);
          if (fd < 0)
            perror("open pid file failed \r\n");
          else {
            snprintf(pid_file, sizeof(pid_file) , "%d" ,getpid());
            if (-1 == write(fd, pid_file, sizeof(pid_file)))
              printf("write pidfile %s failed: %s", app_launcher.pidfile, strerror(errno));
          }
          safe_close(fd);
        }

        /*
         * Wait for early_driver
         */
        if (app_launcher.wait) {
          printf("app %s waiting for %s ...\r\n", app_launcher.appname, app_launcher.wait);
//          for (i = 0; i < 30; i++) {
          while(1) { /* TODO: find a finite value for wait */
            if (-1 != access(app_launcher.wait, F_OK))
              break;
            usleep(5000);
          }
        }

        if (app_launcher.usleep > 0)
          usleep(app_launcher.usleep);

        app_launcher.env[app_launcher.env_used] =
          (char*)"LD_LIBRARY_PATH=/vendor_early_services/system/lib64";
        app_launcher.env_used++;
        app_launcher.argv[app_launcher.argv_used] = NULL;
        app_launcher.env[app_launcher.env_used] = NULL;

      //  write_smack_label(SMACK_LABEL);

        if (app_launcher.username) {
          enforce_user(app_launcher.username);
        }
        if (app_launcher.group) {
          enforce_group(app_launcher.group);
        }
        if (app_launcher.cmd) {
          if ((ret = access(app_launcher.cmd, F_OK)) != 0) {
            LOG(WARNING) << "ES : App " << app_launcher.appname << " doesn't exist ret " << ret << " err " << errno;
            exit(0);
          }

          if (app_launcher.selabel) {
            setexeccon(app_launcher.selabel);
          }

          // load kmod, if applicable for early app
          load_kmod_and_nodes(app_launcher.appname);

#ifdef PLATFORM_GEN4
          if (strcmp(app_launcher.appname, "esplash")) {
#endif /* PLATFORM_GEN4 */
          memset(marker, 0, 50);
          snprintf(marker, 49 ,"M - Launch %s app", app_launcher.appname);
          write_marker(marker);
          LOG(INFO) << "ES : Launching app " << app_launcher.appname;
          ret = execvpe(app_launcher.cmd,app_launcher.argv,app_launcher.env);
          if(ret < 0) {
            LOG(INFO) << "ES : App launch failed " << app_launcher.appname << " errno " << errno;
            memset(marker, 0, 50);
            snprintf(marker, 49 ,"M - Launch %s app failed %d", app_launcher.appname, errno);
            write_marker(marker);
          }
#ifdef PLATFORM_GEN4
          }
#endif /* PLATFORM_GEN4 */
        }
        _exit(0);
      }

      printf("fire up %s \r\n", app_launcher.appname);
      break;
    default:
      printf("unknown config line %s\r\n", p);
  }

out:
  return pid;
}

#ifdef EARLYINIT_DEBUG
/*
 * Check if line is empty or not
 */
static inline bool is_empty_line(const char* p)
{
  return (strspn(p, WHITESPACE) == strlen(p));
}
#endif

#ifdef LV_CODE
static inline void trigger_firmware_loading(const char* path)
{
  int i = 0;
  int fd = -1;
  pid_t pid;
  static char marker[50];

  pid = fork();
  if (pid < 0) {
    perror("fork child process failed \r\n");
    return;
  }
  if (pid == 0) {
    memset(marker, 0, 50);
    snprintf(marker, 49 ,"open-%s-begin", path);
    for (i = 0; i < 30; i++) {
      if (-1 != access(path, F_OK))
        break;
      usleep(5000);
    }
    write_marker(marker);
    fd = open(path, O_CLOEXEC);
    if (fd > 0) {
      memset(marker, 0, 50);
      snprintf(marker, 49 ,"open-%s-end", path);
      write_marker(marker);
    } else {
      perror("open card0 failed");
    }
    safe_close(fd);
    exit(0);
  }
  return;
}
#endif

// Get Console logs enabled config from cmdline or bootconfig
bool kcmd_bc_console_enabled(void)
{
  bool enabled = false, found = false;

  android::earlyinit::import_kernel_cmdline(false,
      [&](const std::string& key, const std::string& value, bool in_qemu) -> bool {
    (void)in_qemu;
    if (key == "console" && value.size() > 0) {
      enabled = true;
      found = true;
    }
    return found;
  });

  if (found)
    return enabled;

  android::earlyinit::import_kernel_bootconfig(false,
     [&](const std::string& key, const std::string& value, bool in_qemu) -> bool {
    (void)in_qemu;
    if (key == "androidboot.console" && value.size() > 0) {
      enabled = true;
      found = true;
    }
    return found;
  });

  return enabled;
}

// Get parallel loading config from bootconfig
bool bc_get_lmp()
{
  bool load_parallel = false, found = false;

  android::earlyinit::import_kernel_bootconfig(false,
     [&](const std::string& key, const std::string& value, bool in_qemu) -> bool {
    (void)in_qemu;
    if (key == "androidboot.load_modules_parallel" && value == "\"true\"") {
      load_parallel = true;
#if defined(__ANDROID_U__)
       load_parallel = false;
#endif
      found = true;
    }
    return found;
  });
#ifdef EARLYINIT_DEBUG
  LOG(INFO) << "ES : Config Modules Parallel load: " << load_parallel;
#endif

  return load_parallel;
}

// Get current boot slot config from bootconfig
bool bc_boot_slot(std::string& slot_suffix)
{
  bool found = false;
  android::earlyinit::import_kernel_bootconfig(false,
     [&](const std::string& key, const std::string& value, bool in_qemu) -> bool {
    (void)in_qemu;
    if (key == "androidboot.slot_suffix") {
      if (value == "\"_a\"") {
        slot_suffix ="_a";
      } else if (value == "\"_b\"") {
        slot_suffix ="_b";
      }
      found = true;
    }
    return found;
  });
#ifdef EARLYINIT_DEBUG
  LOG(INFO) << "ES : Slot suffix: " << slot_suffix;
#endif
  return found;
}

// Get audio reach config from bootconfig
bool bc_get_ar() {
  bool audio_reach = false;
  bool found = false;

  android::earlyinit::import_kernel_bootconfig(false,
     [&](const std::string& key, const std::string& value, bool in_qemu) -> bool {
    (void)in_qemu;
    if (key == "androidboot.audio" && value == "\"audioreach\"") {
      audio_reach = true;
      found = true;
    }
    return found;
  });
#ifdef EARLYINIT_DEBUG
  LOG(INFO) << "ES : Config Audio Reach: " << audio_reach;
#endif

  return audio_reach;
}

EnforcingStatus bc_get_se()
{
  EnforcingStatus status = SELINUX_ENFORCING;
  bool found = false;
  android::earlyinit::import_kernel_bootconfig(false,
    [&](const std::string& key, const std::string& value, bool in_qemu) -> bool {
    (void)in_qemu;
    if (key == "androidboot.selinux" && value == "\"permissive\"") {
      status = SELINUX_PERMISSIVE;
      found = true;
    }
    return found;
  });
  LOG(INFO) << "ES : Selinux mode: " << status;

  return status;
}

bool IsEnforcing()
{
  return bc_get_se() == SELINUX_ENFORCING;
}

void set_permissions(const char *path, int permissions, int user, int group, const char *context)
{
  int ret;
  ret = chmod(path, permissions);
  if(ret){
    LOG(INFO) << "ES: chmod failed for " << path << " errno: " << errno;
  }
  ret = chown(path, user, group);
  if(ret){
    LOG(INFO) << "ES: chown failed for " << path << " errno: " << errno;
  }
  ret = setfilecon(path, context);
  if(ret){
    LOG(INFO) << "ES: setfilecon failed for " << path << " errno: " << errno;
  }
}

#ifdef EARLYINIT_DEBUG
int listDir(char *dirName)
{
       DIR* dir;
       struct dirent *dirEntry;
       struct stat inode;
       char name[1000];
       dir = opendir(dirName);
       freopen("/dev/kmsg", "w", stdout);
       if (dir == 0) {
       perror ("Open failed");
       return -1;
       }
       while ((dirEntry=readdir(dir)) != 0) {
       snprintf(name,sizeof(name),"%s/%s",dirName,dirEntry->d_name);
       lstat (name, &inode);
       if (S_ISDIR(inode.st_mode))
       printf("dir: ");
       else if (S_ISREG(inode.st_mode))
               printf ("file: ");
       else if (S_ISLNK(inode.st_mode))
               printf ("lnk: ");
       else;
       }
return 0;
}
#endif

int getSysInfo(const char * fileName, char * strName)
{
  int fd,ret;

  fd = open(fileName, O_RDONLY);

  if (fd > 0)
  {
      ret = read(fd, strName, sizeof(strName) - 1);
      if (-1 == ret)
      {
        perror("read getSysInfo failed.\r\n");
        return -1;
      }
      close(fd);
      if(ret > 3)
        fd = open("/dev/socket/camera/soc_id", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      else
        fd = open("/dev/socket/camera/platform_subtype_id", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

      if (fd > 0) {
        write(fd,strName, strlen(strName) -1 );
        close(fd);
      } else {
        LOG(INFO) << "/dev/socket/camera/* open failed fd " << fd << " err " << errno;
      }
  }

  return 0;
}

// Wait for availability of file for given msec*count time
static int wait_for_file(const char* file, int sleep_msec, int count, bool log_fail)
{
  int i, ret;
  int delay = sleep_msec * 1000;

  for (i = 0, ret = -1; i < count; i++) {
    if (access(file, F_OK) == 0) {
      ret = 0;
      break;
    }
    usleep(delay);
  }

  if (log_fail) {
    if (ret < 0) {
      LOG(INFO) << "ES File '" << file << "' not found!";
    } else {
#ifdef EARLYINIT_DEBUG
      LOG(INFO) << "ES File '" << file << "' found!";
#endif
    }
  }

  return ret;
}

// Wait for pid until exit or unavailable
static int wait_for_pid(pid_t& pid, int sleep_usec, int count)
{
  pid_t wpid;
  int wstatus;
  int i = 0;

  do {
    wpid = waitpid(pid, &wstatus, 0);
    if (wpid == -1 || wpid != 0 || i++ > count) break;
    usleep(sleep_usec);
  } while (wpid == 0);

#ifdef EARLYINIT_DEBUG
  if (wpid != -1 && wpid == 0) {
    LOG(INFO) << "ES wait pid '" << pid << "'failed!";
  }
#endif

  return 0;
}

static int wait_for_display_ready(int sleep_msec, int count)
{
  int i, ret = -1;
  int delay = sleep_msec * 1000;

  for (i = 0; i < count; i++) {
    if (check_display_driver_ready()) {
      ret = 0;
      break;
    }
    usleep(delay);
  }

  if (ret == -1) {
    LOG(ERROR) << "ES : Wait for Display failed!";
  }

  return ret;
}

int get_device_major_minor(const std::string& uevent_file, int *major, int *minor)
{
  int fd;
  const char *cstr = NULL;
  *major = 0, *minor = 0;

  fd = open(uevent_file.data(), O_WRONLY);
  if (fd >= 0) {
    write(fd, "add\n", 4);
    close(fd);
  }

  std::string ueven_contents;
  if (!android::base::ReadFileToString(uevent_file, &ueven_contents, false))
    return 0;

  std::vector<std::string> lines = android::base::Split(ueven_contents, "\n");
  for (const std::string line : lines) {
    if (line.empty())
      continue;
    cstr = line.data();
    if (strncmp(cstr, "MAJOR=", 6) == 0) {
        cstr += 6;
        *major = atoi(cstr);
    } else if (strncmp(cstr, "MINOR=", 6) == 0) {
        cstr += 6;
        *minor = atoi(cstr);
    }
  }

  LOG(INFO) << "ES device uevent "<< uevent_file <<" - "<< *major << ":" << *minor;
  if (*major > 0) {
    return 1;
  } else {
    return 0;
  }
}

static int check_dma_heap_device_ready(void)
{
  //camera
  static int dma_heap_device_created = 0;
  int major = 0, minor = 0;

  if (!dma_heap_device_created) {
    if (access("/sys/class/dma_heap/qcom,system/uevent", F_OK) == 0) {
      if(get_device_major_minor("/sys/class/dma_heap/qcom,system/uevent", &major, &minor))
      {
        mkdir(DMA_HEAP_DIR, 0666);
        mknod(CAMERA_DMA_HEAP_PATH, S_IFCHR | 0666,
            makedev(major, minor));

        set_permissions(DMA_HEAP_DIR, 0755, AID_ROOT,
            AID_ROOT, "u:object_r:dmabuf_heap_device:s0");
        set_permissions(CAMERA_DMA_HEAP_PATH, 0666, AID_SYSTEM,
            AID_SYSTEM, "u:object_r:vendor_dmabuf_system_heap_device:s0");
        dma_heap_device_created = 1;
        LOG(INFO) << "ES camera dma_heap device nodes ready";
        write_marker("M - EarlyInit dma heap nodes ready");
      }
    }
  }

  return dma_heap_device_created;
}

static int check_gfx_device_ready(void)
{
  //rvc

  static int gfx_device_created = 0;
  int major = 0, minor = 0;

  if (!gfx_device_created) {
    if (access("/sys/class/kgsl/kgsl-3d0/uevent", F_OK) == 0) {
      if(get_device_major_minor("/sys/class/kgsl/kgsl-3d0/uevent", &major, &minor)) {
        mknod("/dev/kgsl-3d0", S_IFCHR | 0666,
            makedev(major, minor));
        set_permissions("/dev/kgsl-3d0", 0666, AID_SYSTEM,
            AID_SYSTEM, "u:object_r:gpu_device:s0");
      }
      gfx_device_created = 1;
      LOG(INFO) << "ES gfx device nodes ready";
      write_marker("M - EarlyInit gfx nodes ready");
    }
  }

  return gfx_device_created;
}

static int check_rvc_device_ready(void)
{
  return (int)(check_gfx_device_ready() && check_dma_heap_device_ready());
}

static int check_ais_device_ready(void)
{
  //rvc
  static int rvc_device_created = 0;
  int major = 0, minor = 0;

  if (!rvc_device_created) {
    if ((access("/sys/bus/media/devices/media0/uevent", F_OK) == 0) &&
        (access("/sys/class/video4linux/video0/uevent", F_OK) == 0) &&
        (access("/sys/class/video4linux/v4l-subdev0/uevent", F_OK) == 0)) {

      LOG(INFO) << "ES check device node for /dev/media0";

      if(get_device_major_minor("/sys/bus/media/devices/media0/uevent", &major, &minor)) {
        mknod("/dev/media0", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/bus/media/devices/media1/uevent", &major, &minor)) {
        mknod("/dev/media1", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/video0/uevent", &major, &minor)) {
        mknod("/dev/video0", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/video1/uevent", &major, &minor)) {
        mknod("/dev/video1", S_IFCHR | 0666,
          makedev(major, minor));
      }

      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev0/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev0", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev1/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev1", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev2/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev2", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev3/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev3", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev4/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev4", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev5/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev5", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev6/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev6", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev7/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev7", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev8/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev8", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev9/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev9", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev10/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev10", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev11/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev11", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev12/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev12", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev13/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev13", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev14/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev14", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev15/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev15", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev16/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev16", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev17/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev17", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev18/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev18", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev19/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev19", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev20/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev20", S_IFCHR | 0666,
          makedev(major, minor));
      }
      if(get_device_major_minor("/sys/class/video4linux/v4l-subdev21/uevent", &major, &minor)) {
        mknod("/dev/v4l-subdev21", S_IFCHR | 0666,
          makedev(major, minor));
      }
      set_camera_media_permission();
      set_camera_video_permission();
      set_camera_v4l_permission();
      rvc_device_created = 1;
      LOG(INFO) << "ES ais device nodes ready";
      write_marker("M - EarlyInit ais nodes ready");
    }
  }

  return rvc_device_created;
}

static int check_video_device_ready(void)
{
  //video
  static int video_device_created = 0;
  int major = 0, minor = 0;

  if (!video_device_created) {
    if ((access("/sys/class/dma_heap/qcom,system/uevent", F_OK) == 0) &&
        (access("/sys/class/video4linux/video32/uevent", F_OK) == 0) &&
        (access("/sys/class/video4linux/video33/uevent", F_OK) == 0)) {
      if (get_device_major_minor("/sys/class/dma_heap/qcom,system/uevent", &major, &minor)) {
        mkdir(DMA_HEAP_DIR, 0666);
        mknod(VIDEO_SYS_DMA_HEAP_PATH, S_IFCHR | 0666,
            makedev(major, minor));
        set_permissions(DMA_HEAP_DIR, 0755, AID_ROOT,
            AID_ROOT, "u:object_r:dmabuf_heap_device:s0");
        set_permissions(VIDEO_SYS_DMA_HEAP_PATH, 0666, AID_SYSTEM,
            AID_SYSTEM, "u:object_r:vendor_dmabuf_system_heap_device:s0");
      }
      if (get_device_major_minor("/sys/class/video4linux/video32/uevent", &major, &minor)) {
        mknod("/dev/video32", S_IFCHR | 0666,
              makedev(major, minor));
      }

      if (get_device_major_minor("/sys/class/video4linux/video33/uevent", &major, &minor)) {
        mknod("/dev/video33", S_IFCHR | 0666,
              makedev(major, minor));
      }

      set_video_permission();
      LOG(INFO) << "ES video device nodes ready";
      write_marker("M - EarlyInit video nodes ready");
      video_device_created = 1;
    }
  }

  return video_device_created;
}

static int check_audio_ar_pkt_ready(void)
{
  static int audio_pkt_device_created = 0;
  int major = 0, minor = 0;

  if (!audio_pkt_device_created) {
    if (access(AUDIO_PKT_PATH, F_OK) == 0) {
      if (get_device_major_minor(AUDIO_PKT_PATH, &major, &minor)) {
        mknod(AUDIO_PKT, S_IFCHR | 0666, makedev(major, minor));
        set_permissions(AUDIO_PKT, 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:vendor_agm_device:s0");
        audio_pkt_device_created = 1;
        LOG(INFO) << "ES AR - ADSP node ready";
        write_marker("AR - ADSP node ready");
      }
    } else {
      LOG(INFO) << "ES AR - ADSP node access denied";
      write_marker("AR - ADSP node access denied");
    }
  }

  return audio_pkt_device_created;
}

static int check_audio_ar_ion_cma_ready(void)
{
  static int msm_ion_cma_device_created = 0;
  int major = 0, minor = 0;

  if (!msm_ion_cma_device_created) {
    if (access(MSM_AUDIO_ION_CMA_PATH, F_OK) == 0) {
      if (get_device_major_minor(MSM_AUDIO_ION_CMA_PATH, &major, &minor)) {
        mknod(MSM_ION_CMA, S_IFCHR | 0666, makedev(major, minor));
        set_permissions(MSM_ION_CMA, 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
        msm_ion_cma_device_created = 1;
        LOG(INFO) << "ES AR - msm ion cma device node ready";
        write_marker("AR - msm ion cma node ready");
      }
    } else {
      LOG(INFO) << "ES AR - msm ion cma device node access denied";
      write_marker("AR - msm ion cma node access denied");
    }
  }
  return msm_ion_cma_device_created;
}

static int check_audio_ar_ml_device_ready(void)
{
  static int audio_ml_device_created = 0;
  int major = 0, minor = 0;

  if (!audio_ml_device_created) {
    if (access(MSM_AUDIO_ML_PATH, F_OK) == 0) {
      if (get_device_major_minor(MSM_AUDIO_ML_PATH, &major, &minor)) {
        mkdir(DMA_HEAP_DIR, 0666);
        set_permissions(DMA_HEAP_DIR, 0755, AID_ROOT,
            AID_ROOT, "u:object_r:dmabuf_heap_device:s0");
        mknod(MSM_AUDIO_ML, S_IFCHR | 0666, makedev(major, minor));
        set_permissions(MSM_AUDIO_ML, 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:vendor_dmabuf_audio_ml_heap_device:s0");
        audio_ml_device_created = 1;
        LOG(INFO) << "ES AR - ml device node ready";
        write_marker("AR - ml node ready");
      }
    } else {
      LOG(INFO) << "ES AR - ml device node access denied";
      write_marker("AR - ml node access denied");
    }
  }
  return audio_ml_device_created;
}

static int check_audio_ar_ion_ready(void)
{
  static int msm_ion_device_created = 0;
  int major = 0, minor = 0;

  if (!msm_ion_device_created) {
    if (access(MSM_AUDIO_ION_PATH, F_OK) == 0) {
      if (get_device_major_minor(MSM_AUDIO_ION_PATH, &major, &minor)) {
        mknod(MSM_ION, S_IFCHR | 0666, makedev(major, minor));
        set_permissions(MSM_ION, 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:vendor_agm_device:s0");
        msm_ion_device_created = 1;
        LOG(INFO) << "ES AR - msm ion node ready";
        write_marker("AR - msm ion node ready");
      }
    } else {
      LOG(INFO) << "ES AR - msm ion node access denied";
      write_marker("AR - msm ion node access denied");
    }
  }

  return msm_ion_device_created;
}

static int check_audio_ar_system_device_ready(void)
{
  static int system_device_created = 0;
  int major = 0, minor = 0;

  if (!system_device_created) {
    if (access(MSM_SYSTEM_PATH, F_OK) == 0) {
      if (get_device_major_minor(MSM_SYSTEM_PATH, &major, &minor)) {
        mkdir(DMA_HEAP_DIR, 0666);
        mknod(MSM_SYSTEM, S_IFCHR | 0666, makedev(major, minor));
        set_permissions(DMA_HEAP_DIR, 0755, AID_ROOT,
            AID_ROOT, "u:object_r:dmabuf_heap_device:s0");
        set_permissions(MSM_SYSTEM, 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:dmabuf_system_heap_device:s0");
        system_device_created = 1;
        LOG(INFO) << "ES AR - system node ready";
        write_marker("AR - system node ready");
      }
    } else {
      LOG(INFO) << "ES AR - system node access denied";
      write_marker("AR - system node access denied");
    }
  }

  return system_device_created;
}

static int check_audio_ar_snd_device_ready(void)
{
  static int audio_snd_device_created = 0;

  if (!audio_snd_device_created) {
    if (access("/sys/kernel/snd_card/card_state", F_OK) == 0) {
      LOG(INFO) << "ES Audio snd node ready";
      write_marker("AR - Audio snd node ready");
      audio_snd_device_created = 1;
    } else {
      LOG(INFO) << "ES AR - snd node access denied";
      write_marker("AR - snd node access denied");
    }
  }

  return audio_snd_device_created;
}

static int check_audio_ar_pcm_device_ready(void)
{
  static int audio_pcm_device_created = 0;

  if (!audio_pcm_device_created) {
    if (access(PCM_PATH, F_OK) == 0) {
      set_permissions(PCM_PATH, 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
      LOG(INFO) << "ES AR - pcm node ready";
      write_marker("AR - pcm node ready");
      audio_pcm_device_created = 1;
    } else {
      LOG(INFO) << "ES AR - pcm node access denied";
      write_marker("AR - pcm access denied");
    }
  }

  return audio_pcm_device_created;
}

static int check_audio_ar_id_device_ready(void)
{
  static int audio_id_device_created = 0;

  if (!audio_id_device_created) {
    if (access(PCM_ID_PATH, F_OK) == 0) {
      set_permissions(PCM_ID_PATH, 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
      LOG(INFO) << "ES AR - card0 id node ready";
      write_marker("AR - card0 id node ready");
      audio_id_device_created = 1;
    } else {
      LOG(INFO) << "ES AR - card0 id node access denied";
      write_marker("AR - card0 id node access denied");
    }
  }

  return audio_id_device_created;
}

static void set_audio_ar_permission(void)
{
  LOG(INFO) << "ES : Set Audio Permissions";
  set_permissions("/dev/snd", 00777, AID_ROOT, AID_ROOT, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/controlC0", 00666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D0p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D1p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D2p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D3p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D4p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D5c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D6c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D7p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D8p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D9c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D10c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D11c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D12p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D13c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D14c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D15c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D16c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D17p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D18p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D19c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D20c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D21p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D22c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D23p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D24c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D25p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D26c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D27p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D28c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D29p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D30c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D31p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D32c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D33p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D34c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D35p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D36p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D37c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");

  return;
}

static int check_audio_ar_device_ready(void)
{
  static int audio_device_created = 0;
  int major = 0, minor = 0;

  //audio
  if (!audio_device_created) {
    write_marker("AR - EarlyInit Audio AR nodes start");
    if (access(MSM_SOUND_CTRL_PATH, F_OK) == 0) {
      get_device_major_minor(MSM_SOUND_CTRL_PATH, &major, &minor);
      if (major == 0)
        return 0;

      mkdir(SND_CARD_DIR, 0755);
      mknod("/dev/snd/controlC0", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D0p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D0p", S_IFCHR | 0660,  makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D1p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D1p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D2p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D2p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D3p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D3p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D4p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D4p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D5c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D5c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D6c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D6c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D7p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D7p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D8p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D8p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D9c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D9c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D10c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D10c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D11c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D11c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D12p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D12p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D13c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D13c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D14c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D14c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D15c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D15c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D16c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D16c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D17p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D17p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D18p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D18p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D19c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D19c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D20c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D20c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D21p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D21p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D22c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D22c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D23p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D23p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D24c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D24c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D25p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D25p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D26c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D26c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D27p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D27p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D28c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D28c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D29p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D29p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D30c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D30c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D31p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D31p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D32c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D32c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D33p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D33p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D34c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D34c", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D35p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D35p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D36p/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D36p", S_IFCHR | 0660, makedev(major, minor));
      get_device_major_minor("/sys/class/sound/pcmC0D37c/uevent", &major, &minor);
      mknod("/dev/snd/pcmC0D37c", S_IFCHR | 0660, makedev(major, minor));
      write_marker("AR - EarlyInit Audio AR nodes ready");
      set_audio_ar_permission();
      LOG(INFO) << "ES AR - Audio AR device nodes ready";
      audio_device_created = 1;
    } else {
        LOG(INFO) << "ES AR - Audio AR device node access denied";
        write_marker("ES AR - Audio AR device nodes access denied");
    }
  }

  return audio_device_created;
}

static int check_and_create_linker64 (void) {
    static int linker_ready = 0;
    if (linker_ready == 0) {
        if (access("/vendor_early_services/system/bin/bootstrap/linker64", F_OK) == 0) {
            mkdirs("/system/bin", 0755);
            if (symlink("/vendor_early_services/system/bin/bootstrap/linker64", "/system/bin/linker64") == 0) {
                linker_ready = 1;
                LOG(INFO) << "symlink /system/bin/linker64 is created";
            } else  {
                LOG(INFO) << "symlink /system/bin/linker64 create failed error " << errno << " " << strerror(errno);
            }
        }
    }
    return linker_ready;
}

static int check_and_create_vendor_etc (void) {
    static int vendor_etc_ready = 0;
    if (vendor_etc_ready == 0) {
        if (access("/vendor_early_services/vendor/etc", F_OK) == 0) {
            mkdirs("/vendor", 0666);
            if (symlink("/vendor_early_services/vendor/etc", "/vendor/etc") == 0) {
                vendor_etc_ready = 1;
                LOG(INFO) << "symlink /vendor/etc is created";
            } else  {
                LOG(INFO) << "symlink /vendor/etc create failed error " << errno << " " << strerror(errno);
            }
        }
    }
    return vendor_etc_ready;
}

static int check_and_create_vendor_firmware (void) {
    static int vendor_etc_ready = 0;
    if (vendor_etc_ready == 0) {
        if (access(AUDIO_FW_PATH, F_OK) == 0) {
            if (symlink(AUDIO_FW_PATH, "/vendor/firmware_mnt") == 0) {
                vendor_etc_ready = 1;
                LOG(INFO) << "symlink /vendor/firmware_mnt is created";
            } else  {
                LOG(INFO) << "symlink /vendor/firmware_mnt create failed error " << errno << " " << strerror(errno);
            }
        }
    }
    return vendor_etc_ready;
}

static int check_audio_ar_ready(void)
{
    return (int)(check_audio_ar_pkt_ready() &&
                 check_audio_ar_ion_ready() &&
                 check_audio_ar_ion_cma_ready() &&
                 check_audio_ar_system_device_ready() &&
                 check_audio_ar_ml_device_ready() &&
                 check_audio_ar_snd_device_ready() &&
                 check_audio_ar_pcm_device_ready() &&
                 check_audio_ar_id_device_ready() &&
                 check_audio_ar_device_ready());
}

static void create_drm_udev_cards(void)
{
  int i = 0;
  int major = 0, minor = 0;
  char buf[128];

  while (i < cards_max) {
    if (!_drm_cards[i].is_created) {
      // check if sysfs entry is created
      if (access(_drm_cards[i].sysfs_path, F_OK) == 0) {
        if (get_device_major_minor(_drm_cards[i].sysfs_path, &major, &minor)) {
          mkdir(_drm_cards[i].udev_dir, 0666);
          mknod(_drm_cards[i].udev_node_path, S_IFCHR | 0666,
                makedev(major, minor));

		  if (i == FAST_RVC_CARD) {
			  set_permissions(_drm_cards[i].udev_dir, 0755,
							  AID_ROOT, AID_ROOT, "u:object_r:device:s0");
			  set_permissions(_drm_cards[i].udev_node_path, 0666,
							  AID_ROOT, AID_CAMERA, "u:object_r:graphics_device:s0");
		  } else {
			  set_permissions(_drm_cards[i].udev_dir, 0755,
							  AID_ROOT, AID_ROOT, "u:object_r:device:s0");
			  set_permissions(_drm_cards[i].udev_node_path, 0666,
							  AID_ROOT, AID_GRAPHICS, "u:object_r:graphics_device:s0");
		  }

	  snprintf(buf, sizeof(buf), "M - EarlyInit /dev/dri/%s ready", _drm_cards[i].num);
	  write_marker(buf);
          _drm_cards[i].is_created = true;
        }
      }
    }
    i++;
  }
}


#if 0
static int check_display_driver_ready(void)
{
  int fd;
  char status[32] = {0};
  static bool dpu0_ready = false;
#ifdef PLATFORM_GEN4
  static bool dpu1_ready = false;
#endif
  bool rc = false;

#ifdef PLATFORM_GEN4
  if (dpu0_ready && dpu1_ready)
#else
  if (dpu0_ready)
#endif
    return true;

  fd = open(DISP_DRM_DPU0_READY_PATH, O_RDONLY);
  if (fd > 0 && read(fd, status, sizeof(status) - 1) > 0) {
    if (status[0] == '1') {
      dpu0_ready = true;
    }
    close(fd);
  }
  rc = dpu0_ready;

#ifdef PLATFORM_GEN4
  fd = open(DISP_DRM_DPU1_READY_PATH, O_RDONLY);
  if (fd > 0 && read(fd, status, sizeof(status) - 1) > 0) {
    if (status[0] == '1') {
      dpu1_ready = true;
    }
    close(fd);
  }
  rc = dpu0_ready & dpu1_ready;
#endif

  return rc;
}
#else
static int check_display_driver_ready(void)
{
	int ret = 0;

	if(access(DISP_DRM_DRIVER_CARD4_READY_PATH, F_OK) == 0 && access(DISP_DRM_DRIVER_RENDER_READY_PATH, F_OK) == 0)
	{
		LOG(INFO) << "Function: " << __func__ << ", Line: " << __LINE__ << " check driver sucess------\n";
		ret = 1;
	}else
	{
		//LOG(INFO) << "Function: " << __func__ << ", Line: " << __LINE__ << " wunatest check driver fail------\n";
		ret = 0;
	}

	return ret;
}
#endif

static int check_storage_device_ready(void)
{
  static int sto_device_created = 0;

  if (!sto_device_created) {
    if (access("/sys/block/sdd/uevent", F_OK) == 0 &&
        access("/sys/block/sde/uevent", F_OK) == 0 &&
        access("/sys/block/sdf/uevent", F_OK) == 0) {
      LOG(INFO) << "ES SD nodes ready";
      write_marker("M - EarlyInit SD nodes ready");
      sto_device_created = 1;
    }
  }

  return sto_device_created;
}

static void trigger_adsp(char * flag) {
    int fd = open("/sys/kernel/boot_adsp/boot", O_WRONLY);
    if (fd < 0) {
        LOG(WARNING) << "ES : trigger ADSP open sys entry failed";
    } else if(-1 == write(fd, flag, 1)) {
        LOG(WARNING) << "ES : trigger ADSP Write to sys entry failed";
    } else {
        write_marker("M - ES Start ADSP");
        LOG(INFO) << "ES : trigger ADSP firmware loading triggered";
    }
    if (fd > 0)
        close(fd);
}

static int check_pdmapper_ready(void)
{
  // pd-mapper
  static int adsp_path_ready = 0;
  if (!adsp_path_ready) {
    if (access(AUDIO_ADSP_FW_PATH, F_OK) == 0) {
      adsp_path_ready = 1;
      LOG(INFO) << "ES pd-mapper ready";
      write_marker("M - EarlyInit pd-mapper ready");
    }
  }

  return adsp_path_ready;
}

static int check_lxc_rootfs_device_ready(void)
{
  const char *lxc_path = "/vendor_early_services/vendor/vm-system/lxc/bin/lxc-start";

  if (access(lxc_path, X_OK) != 0) {
    freopen("/dev/kmsg", "w", stdout);
    printf(" ES : No lxc-start, it means vm-bootsys is not mounted yet");
    return 0;
  } else {
    freopen("/dev/kmsg", "w", stdout);
    printf(" ES : lxc-start exist, vm-bootsys has been mounted!");
    return 1;
  }
}

static int check_lxc_device_ready(void)
{
  return (int)(check_lxc_rootfs_device_ready() && check_display_driver_ready() && check_gfx_device_ready() && check_dma_heap_device_ready() && check_video_device_ready());
}

#ifdef ES_AUDIOE_DISABLED
#define SND_CARD_DIR "/dev/snd"
static int check_audio_device_ready(void)
{
  // audio
  static int audio_device_created = 0;
  int major = 0, minor = 0;

  if (!audio_device_created) {
    if ((access("/sys/class/sound/controlC0/uevent", F_OK) == 0) &&
        (access("/sys/class/sound/pcmC0D49c/uevent", F_OK) == 0) &&
        (access("/sys/class/sound/pcmC0D48p/uevent", F_OK) == 0) &&
        (access("/sys/class/sound/pcmC0D50p/uevent", F_OK) == 0)) {
      mkdir(SND_CARD_DIR, 0666);
      if (get_device_major_minor("/sys/class/sound/controlC0/uevent", &major, &minor)) {
        mknod("/dev/snd/controlC0", S_IFCHR | 0666, makedev(major, minor));
      }
      if (get_device_major_minor("/sys/class/sound/pcmC0D49c/uevent", &major, &minor)) {
        mknod("/dev/snd/pcmC0D49c", S_IFCHR | 0666, makedev(major, minor));
      }
      if (get_device_major_minor("/sys/class/sound/pcmC0D48p/uevent", &major, &minor)) {
        mknod("/dev/snd/pcmC0D48p", S_IFCHR | 0666, makedev(major, minor));
      }
      if (get_device_major_minor("/sys/class/sound/pcmC0D50p/uevent", &major, &minor)) {
        mknod("/dev/snd/pcmC0D50p", S_IFCHR | 0666, makedev(major, minor));
      }
      if (get_device_major_minor("/sys/class/sound/pcmC0D53c/uevent", &major, &minor)) {
        mknod("/dev/snd/pcmC0D53c", S_IFCHR | 0666, makedev(major, minor));
      }
      if (get_device_major_minor("/sys/class/sound/pcmC0D55p/uevent", &major, &minor)) {
        mknod("/dev/snd/pcmC0D55p", S_IFCHR | 0666, makedev(major, minor));
      }
      set_audio_permission();
      LOG(INFO) << "ES audio device nodes ready";
      write_marker("M - EarlyInit audio nodes ready");
      audio_device_created = 1;
    }
  }

  return audio_device_created;
}

static void set_audio_permission(void)
{
  LOG(INFO) << "ES : Set Audio Permissions";
  set_permissions("/dev/snd", 0777, AID_ROOT, AID_ROOT, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/controlC0", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D49c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D48p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D50p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D53c", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D55p", 0666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");

  return;
}
#endif // ES_AUDIOE_DISABLED

static void set_video_permission(void)
{
  set_permissions(VIDEO32_DEVICE_PATH, 0666, AID_ROOT, AID_GRAPHICS, "u:object_r:video_device:s0");
  set_permissions(VIDEO33_DEVICE_PATH, 0666, AID_ROOT, AID_GRAPHICS, "u:object_r:video_device:s0");
  LOG(INFO) << "EarlyVideo Setting permission to video device completed";
  return;
}

static void set_camera_media_permission(void)
{
  LOG(INFO) << "ES : Set Camera Permissions for mdev";
  set_permissions("/dev/media0", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/media1", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  LOG(INFO) << "ES : Set Camera Permissions Completed for mdev";
  return;
}

static void set_camera_video_permission(void)
{
  LOG(INFO) << "ES : Set Camera Permissions for vdev";
  set_permissions("/dev/video0", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/video1", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  LOG(INFO) << "ES : Set Camera Permissions Completed for vdev";
  return;
}

static void set_camera_v4l_permission(void)
{
  LOG(INFO) << "ES : Set Camera Permissions for v4l-subdev";
  set_permissions("/dev/v4l-subdev1", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev2", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev3", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev4", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev5", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev6", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev7", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev8", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev9", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev10", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  //For directories, only ROOT is able to apply permissions at ES stage.
  set_permissions("/dev/socket/camera", 0775, AID_ROOT, AID_CAMERA, "u:object_r:vendor_camera_socket:s0");
  selinux_android_restorecon("/dev/socket/camera", SELINUX_ANDROID_RESTORECON_RECURSE);
  set_permissions("/dev/v4l-subdev11", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev12", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev13", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev14", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev15", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev16", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev17", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev18", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev19", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev20", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev21", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev0", 0666, AID_CAMERA, AID_CAMERA, "u:object_r:video_device:s0");
  LOG(INFO) << "ES : Set Camera Permissions Completed for v4l-subdev";

  return;
}

// Check uvent and
// 1. set_km: initiate dev enumerations
// 2. !set_km: mount fw partition.
static int prepare_fw_dir(bool set_km)
{
  // FW mount partition
  std::string modemStr("/dev/block/by-name/modem");
  std::string lxcrootfsStr("/dev/block/by-name/vm-bootsys");
  unsigned int count = 0, max = (WAIT_SET_PERM_SECS * 1000) / WAIT_SLEEP_MSEC;
  boot_clock::time_point module_start_time = boot_clock::now();
  bool mounted = false;
  bool lxcmounted = false;
  if (_use_min_wait) {
    max = (WAIT_SET_PERM_MSECS) / WAIT_SLEEP_MSEC;
  }

  while (count++ < max) {
    if (check_storage_device_ready())
      break;
    usleep(WAIT_SLEEP_MSEC * 1000);
  }
  if (set_km) {
    // Enumerate dev nodes - fw
    mknod("/dev/kmdone", S_IFREG | 0400, makedev(0,0));

    return 0;
  }

  if (access(AUDIO_FW_PATH, F_OK) == -1) {
    LOG(WARNING) << "ES : AUDIO_FW_PATH doesn't exist";
    mkdirs(AUDIO_FW_PATH, 0755);
  }

  modemStr += _boot_slot;
  // wait for node creation
  if (wait_for_file(modemStr.c_str(), WAIT_SLEEP_MSEC, max*2) == 0) {
    // mount partition
    if (mount(modemStr.c_str(), AUDIO_FW_PATH, "vfat",
      MS_RDONLY, "uid=1000,gid=1000,dmask=227,fmask=337,context=u:object_r:firmware_file:s0") < 0) {
      LOG(WARNING) << "ES : modemstr mount failed, err " << errno;
    } else {
      LOG(INFO) << "ES : modemstr mount success.";
      mounted = true;
      print_log("modemstr mount success");
    }
  } else {
    LOG(WARNING) << "ES : modemstr Not Found!";
  }

  lxcrootfsStr += _boot_slot;
  const char *lxc_start_file = "/vendor_early_services/vendor/vm-system/lxc/bin/lxc-start";

  // wait for node creation
  if (wait_for_file(lxcrootfsStr.c_str(), WAIT_SLEEP_MSEC, max*2) == 0) {
    // mount partition
    //if (mount(lxcrootfsStr.c_str(), LXC_ROOTFS_PATH, "ext4",
    //  MS_RDONLY, NULL) < 0) {
      if (wait_for_file(lxc_start_file, WAIT_SLEEP_MSEC, max*2)) {
      LOG(WARNING) << "ES : lxc rootfs mount failed, err " << errno;
    } else {
      LOG(INFO) << "ES : lxc rootfs mount success.";
      lxcmounted = true;
      print_log("lxc rootfs mount success");
    }
  } else {
    LOG(WARNING) << "ES : lxc rootfs Not Found!";
  }

  char str[SHORT_STRING_MAX] = {0};
  auto module_elapse_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                boot_clock::now() - module_start_time);
  if (mounted && lxcmounted) {
    snprintf(str, SHORT_STRING_MAX, "%s%d%s", "M - ES fw-load took ",
           (int)module_elapse_time.count(), "ms");
  } else {
    snprintf(str, SHORT_STRING_MAX, "%s%d%s", "M - ES fw-load FAILED, waited ",
           (int)module_elapse_time.count(), "ms");
  }
  write_marker(str);

  return 0;
}

#ifdef __ANDROID_U__
static int es_selinux_android_load_policy_from_fd(int fd, const char *description)
{
  int rc;
  struct stat sb;
  void *map = NULL;
  static int load_successful = 0;

  LOG(INFO) << "ES SELinux: Load Sepolicy from fd";
  if (load_successful){
    LOG(INFO) << "ES SELinux: Attempted reload of SELinux policy!";
    return 0;
  }
  set_selinuxmnt(SELINUXMNT);
  if (fstat(fd, &sb) < 0) {
    LOG(INFO) << "ES SELinux:  Could not stat " << description << " Error: " <<  strerror(errno);
    return -1;
  }
  map = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (map == MAP_FAILED) {
    LOG(INFO) << "ES SELinux:  Could not map " << description << " Error: " <<  strerror(errno);
    return -1;
  }

  rc = security_load_policy(map, sb.st_size);
  if (rc < 0) {
    LOG(INFO) << "ES SELinux:  Could not load policy " << description << " Error: " <<  strerror(errno);
    munmap(map, sb.st_size);
    return -1;
  }

  munmap(map, sb.st_size);
  load_successful = 1;
  LOG(INFO) << "ES SELinux: es_selinux_android_load_policy_from_fd rc : " << rc;
  return 0;
}
#endif //__ANDROID_U__

static int load_precompiled_sepolicy()
{
  boot_clock::time_point module_start_time = boot_clock::now();
  // Load the vendor early service policy
  std::string precompiled_sepolicy_file = EARLY_SERVICES_SEPOL;

  LOG(INFO) << "ES : Load  precompiled sepolicy ";
  int fd1 = open(precompiled_sepolicy_file.c_str(),
                 O_RDONLY | O_CLOEXEC | O_BINARY);
  if (fd1 > 0) {
    if (
#ifdef __ANDROID_U__
      es_selinux_android_load_policy_from_fd(fd1,
#else
      selinux_android_load_policy_from_fd(fd1,
#endif
        precompiled_sepolicy_file.c_str()) < 0) {
      LOG(WARNING) << "ES : Failed to load SELinux policy " << precompiled_sepolicy_file.c_str();
    } else {
      LOG(INFO) << "ES : Successfully loaded precompiled sepolicy";
    }
    close(fd1);
  }
  char str[SHORT_STRING_MAX] = {0};
  auto module_elapse_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                boot_clock::now() - module_start_time);
  snprintf(str, SHORT_STRING_MAX, "%s%d%s", "M - ES Sepolicy-load took ",
           (int)module_elapse_time.count(), "ms");
  write_marker(str);

  return 0;
}

// Based on info for appname in _eapp_info
// 1. Load kernel modules associated with appname.
// 2. Wait for states EAPP_WAIT_*/EAPP_MOD_WAIT_*, if required.
// 3. Wait for ready states (devnode, events, permisions etc), if required
static int load_kmod_and_nodes(const char* appname)
{
  pid_t pid;
  int i;
  unsigned int count, max;
  char str[SHORT_STRING_MAX];

#ifdef EARLYINIT_DEBUG
  int chk = 0;

  LOG(INFO) << "ES : Load kmod and wait node for " << appname;
#endif
  // Add null check for appname to fix kw issue.
  if (appname == NULL) {
    LOG(INFO) << "ES : appname NULL pointer\n ";
    return -1;
  }

  // get the max value based on build variant
  max = (_use_min_wait)?(WAIT_SET_PERM_MSECS / WAIT_SLEEP_MSEC):
                      ((WAIT_SET_PERM_SECS * 1000) / WAIT_SLEEP_MSEC);
  for (i = 0; _eapp_info[i].name[0]; i++) {
    if (!strncmp(appname, _eapp_info[i].name, sizeof(_eapp_info[i].name)-1)) {
      break;
    }
  }


  // Wait for Display for all apps, if not set too
  if (_eapp_info[i].wait != EAPP_WAIT_NONE && _eapp_info[i].wait != EAPP_WAIT_DISP) {
    wait_for_display_ready(WAIT_SLEEP_MSEC, max*2);
  }


  // Wait for FW availability if set
  if (_eapp_info[i].wait == EAPP_WAIT_DEFAULT || _eapp_info[i].wait & EAPP_MOD_WAIT_FW) {
    wait_for_file((char*)ES_FW_CHK_PATH, WAIT_SLEEP_MSEC, max*2);
    if (_eapp_info[i].name[0] == 0)
      return 0;
  }

  // Initate Load modules
  if (_eapp_info[i].kfile[0] != 0) {
    snprintf(str, SHORT_STRING_MAX ,"M - Load mod-node %s", appname);
    write_marker(str);

    if ((pid = fork()) == 0) {
      LOG(INFO) << "ES : Fork for mmmod " << appname;
      setexeccon("u:r:vendor_init:s0");
      const char *path = "/vendor_early_services/bin/early_services_init";
      snprintf(str, SHORT_STRING_MAX, "%d", i);

      const char *args[] = { path, "mmmod", str, NULL };
      execv(path, const_cast<char**>(args));
      LOG(WARNING) << "ES : Exec for mmmod, failed!";
      _exit(0);
    }

    int pmax = (_use_min_wait)?((WAIT_PID_MIN_MSECS * 1000) / WAIT_SLEEP_USECS):
                      ((WAIT_PID_MAX_MSECS * 1000) / WAIT_SLEEP_USECS);
    wait_for_pid(pid, WAIT_SLEEP_USECS, pmax);
  }

  // Wait if ready not set
  if (_eapp_info[i].is_ready == NULL) {
    if (_eapp_info[i].wait & EAPP_WAIT_DISP) {
      wait_for_display_ready(WAIT_SLEEP_MSEC, max*2);
      create_drm_udev_cards();
    }
    return 0;
  }

  // Wait for Display - specific case where we have to wait here after kmod load
  if (_eapp_info[i].wait & EAPP_WAIT_DISP) {
    wait_for_display_ready(WAIT_SLEEP_MSEC, max*2);
    create_drm_udev_cards();
  }

  LOG(INFO) << "ES : wait for ready, app " << _eapp_info[i].tag
            << " iter max " << max;

  count = 0;
  bool ready = false;
  // wait for device/perm ready
  while (count++ < max) {
    if (_eapp_info[i].is_ready()) {
      ready = true;
      break;
    }
#ifdef EARLYINIT_DEBUG
    if (chk++ %30 ==0)
      LOG(INFO) << "ES : wait and set check perm " << _eapp_info[i].tag;
#endif
    usleep(WAIT_SLEEP_MSEC * 1000);
  }

  if (ready) {
    LOG(INFO) << "ES : wait for ready took " << (count * WAIT_SLEEP_MSEC)/1000
            << "s ready " << ready <<" app " << _eapp_info[i].tag;
  } else {
    char str[SHORT_STRING_MAX] = {0};
    snprintf(str, SHORT_STRING_MAX, "M - ES %s ready FAILED", _eapp_info[i].tag);
    write_marker(str);
    LOG(INFO) << "ES : FAILED - wait for ready took " << (count * WAIT_SLEEP_MSEC)/1000
            << "s ready " << ready <<" app " << _eapp_info[i].tag;
  }

  return 0;
}

#define MAX_MODULES_PER_LINE 64
static int load_modules_parallel(const std::string& fl,
                   const std::string& mod_path, const int th_count,
                   const std::string& logtag, int flag)
{
  boot_clock::time_point module_start_time = boot_clock::now();
  const int TH_MAX = th_count;
  std::string mlist;
  int load_count = 0;
  int fail_count = 0;
  static const char ADSP_KO[] = ADSP_LOADER_KO;
  LOG(INFO) << "start Loading modules ";
  if (!android::base::ReadFileToString(fl, &mlist, false))
    return -1;

#ifdef EARLYINIT_DEBUG
  LOG(INFO) << "Loading modules " << mod_path << " file " << fl;
#endif

  std::vector<std::string> lines = android::base::Split(mlist, "\n");
  for (const std::string line : lines) {
    if (line.empty())
      continue;

    std::vector<std::thread> th_mods;
    std::mutex mods_lock;
    char mline[LINE_MAX] = {0};
    char *kmod[MAX_MODULES_PER_LINE] = {0};
    int i = 0, len = 0;
    const char* ptr = line.c_str();
    if (ptr == NULL)
      continue;

    kmod[len] = &mline[0];
    // split the words as C strings
    for (; i < (LINE_MAX-1) && (*ptr != 0 && len < MAX_MODULES_PER_LINE); i++, ptr++) {
      if (*ptr != ' ') {
         mline[i] = *ptr;
      } else {
         mline[i] = 0;
         kmod[++len] = &mline[i+1];
      }
    }
    if (i) {
      mline[i] = 0;
      len++;
    }

    if (len < 1)
       continue;

    int num_threads = (len > TH_MAX)?TH_MAX:len;
    i = -1;
    auto mod_load_thread_fn = [&] {
      std::unique_lock lk(mods_lock);
      while (++i < len) {
        char fl[SHORT_STRING_MAX];
        int j = i;
        if (kmod[j][0] == 0) {
          continue;
        }
        load_count++;
        lk.unlock();
        if (flag == LMP_MODPROBE) {
          if (android::earlyinit::insert_kernel_module(kmod[j]) == false) {
            fail_count++;
          }
          lk.lock();
          continue;
        }
        snprintf(fl, SHORT_STRING_MAX, "%s%s.ko", mod_path.c_str(), kmod[j]);
        int fd = open(fl, O_RDONLY);
        if (fd > 0) {
          std::string param;
          android::earlyinit::get_kernel_module_param(kmod[j], param, _module_params);
          int ret = finit_module(fd, param.c_str(), 0);
          if (ret < 0 && errno != EEXIST) {
            LOG(WARNING) << "fd = " << fd << "ES : init_module failed " << fl << " errno: " << errno;
            fail_count++;
          } else {
#ifdef EARLYINIT_DEBUG
            LOG(INFO) << "ES : init_module success for: " << fl;
#endif
          }
          close(fd);

          // Check for audio
          if (flag == LMP_DIRECT_CHK_AUD
             && !strncmp(kmod[j], ADSP_KO, sizeof(ADSP_KO)-1)) {
              trigger_adsp("1");
          }
        } else {
          fail_count++;
          LOG(WARNING) << "ES : Failed to open module " << fl;
        }
        lk.lock();
      }
      usleep(5);
    };

    std::generate_n(std::back_inserter(th_mods), num_threads,
                    [&] { return std::thread(mod_load_thread_fn); });

    for (auto& th : th_mods) {
      th.join();
    }
  }

  char str[SHORT_STRING_MAX] = {0};
  auto module_elapse_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                boot_clock::now() - module_start_time);
  if (fail_count == 0) {
    snprintf(str, SHORT_STRING_MAX, "M - ES %s-mod took %d%s", logtag.c_str(),
          (int)module_elapse_time.count(), "ms");
  } else {
    snprintf(str, SHORT_STRING_MAX, "M - ES %s-mod took %d%s Fail %d/%d", logtag.c_str(),
          (int)module_elapse_time.count(), "ms", fail_count, load_count);
  }

  write_marker(str);

  LOG(INFO) << "ES : Load modules done " << logtag << " count " << load_count
     << " Failed: " << fail_count;

  return 0;
}

#if defined(__ANDROID_U__)
static void launch_test_app(void)
{
  int fd;
  pid_t pid = -1;
  int ret = -1;
  char pid_file[10] = {0};
  static char marker[50];

  LOG(INFO) << "Launch Test APP";
  app_launcher_start_over();
  app_launcher.appname = strdup(TEST_APP);
  app_launcher.cmd = strdup(TEST_APP_CMD);
  app_launcher.argv[app_launcher.argv_used] = strdup(TEST_APP_CMD);
  app_launcher.argv_used++;
  app_launcher.applog = strdup(TEST_APP_LOG);
  app_launcher.env[app_launcher.env_used] = strdup(TEST_APP_ENV);
  app_launcher.env_used++;
  app_launcher.pidfile = strdup(TEST_APP_PID);

  pid = fork();
  if (pid < 0) {
     LOG(INFO) << " early_init fork child process failed ";
     perror("fork child process failed \r\n");
     return;

  }
  if (0 == pid) {
  if (app_launcher.applog) {
    fd = open(app_launcher.applog, O_RDWR | O_CREAT, 0666);
    if (fd > 0) {
       dup2(fd, fileno(stdout));
       dup2(fd, fileno(stderr));
       safe_close(fd);
       safe_close(fd);
     }
  } else {
    fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
  }
  if (app_launcher.pidfile) {
    fd = open(app_launcher.pidfile, O_WRONLY | O_CREAT, 0666);
    if (fd < 0)
       perror("open pid file failed \r\n");
     else {
         snprintf(pid_file, sizeof(pid_file) , "%d" ,getpid());
         if (-1 == write(fd, pid_file, sizeof(pid_file)))
        printf("write pidfile %s failed: %s", app_launcher.pidfile, strerror(errno));
    }
    safe_close(fd);
  }

  if (app_launcher.wait) {
    printf("app %s waiting for %s ...\r\n", app_launcher.appname, app_launcher.wait);
    while(1) { /* TODO: find a finite value for wait */
      if (-1 != access(app_launcher.wait, F_OK))
        break;
        usleep(5000);
     }
  }

  app_launcher.env[app_launcher.env_used] = (char *)"LD_LIBRARY_PATH=/vendor_early_services/system/lib64";
  app_launcher.env_used++;
  app_launcher.argv[app_launcher.argv_used] = NULL;
  app_launcher.env[app_launcher.env_used] = NULL;

  if (app_launcher.username) {
     enforce_user(app_launcher.username);
  }
  if (app_launcher.group) {
    enforce_group(app_launcher.group);
  }
  if (app_launcher.cmd) {
    if ((ret = access(app_launcher.cmd, F_OK)) != 0) {
       LOG(WARNING) << "ES : App " << app_launcher.appname << " doesn't exist ret " << ret << " err " << errno;
       return;
  }
  memset(marker, 0, 50);
  snprintf(marker, 49 ,"M - Launch %s app", app_launcher.appname);
  write_marker(marker);
  LOG(INFO) << "ES : Launching app " << app_launcher.appname;
  ret = execvpe(app_launcher.cmd,app_launcher.argv,app_launcher.env);
  if(ret < 0) {
    LOG(INFO) << "ES : App launch failed " << app_launcher.appname << " errno " << errno;
    memset(marker, 0, 50);
    snprintf(marker, 49 ,"M - Launch %s app failed %d", app_launcher.appname, errno);
    write_marker(marker);
    }
  }
  }
}

#endif

static void launch_early_apps(void)
{
#ifdef __ANDROID_U__
  std::string fl = ANDROID_U_CONF;
#elif PLATFORM_GEN4
  std::string fl = ES_GEN4_CONF;
#else
  std::string fl = DEFAULT_CONF;
#endif // __ANDROID_U__
  std::string list;

  if (!android::base::ReadFileToString(fl, &list, false)) {
    LOG(ERROR) << "Unable to read conf file";
    return;
  }

  std::vector<std::string> lines = android::base::Split(list, "\n");
  char buf[LINE_MAX];
  pid_t pid;
  int i = 0;
  for (const std::string line : lines) {
    android::base::Trim(line);
    if (line.empty())
      continue;

    strlcpy(buf, line.c_str(), sizeof(buf));
    if ((pid = parse_line(buf)) > 0) {
      if (i < EAPPS_MAX) {
        _eapp_pid[i++] = pid;
#ifdef EARLYINIT_DEBUG
        LOG(INFO) << "ES: eappid " << pid;
#endif
      }
    }
    memset(buf, 0, sizeof(buf));
  }
  if (i == EAPPS_MAX)
    LOG(WARNING) << "ES : Max Apps limit reached!";
}

#if defined(__ANDROID_U__)
static int load_default_modules()
{
  int count = 0;
  boot_clock::time_point module_start_time = boot_clock::now();
  android::earlyinit::load_kernel_modules(count, bc_get_lmp());
  LOG(INFO) << "ES : Modules loaded count " << count;
  auto module_elapse_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                     boot_clock::now() - module_start_time);
  char str[SHORT_STRING_MAX] = {0};
  snprintf(str, SHORT_STRING_MAX, "%s%d%s", "M - ES def-mod took ",
               (int)module_elapse_time.count(), "ms");
  write_marker(str);
  return 0;
}
#endif // __ANDROID_U__

static int wait_for_early_apps(void)
{
  char comm[SHORT_STRING_MAX/2];
  char comm_path[SHORT_STRING_MAX/2];
  unsigned int count = 0, max;
  int i, ret, fd, status;

  // get the max value based on build variant
  max = (_use_min_wait)?(WAIT_EAPP_MSECS/WAIT_SLEEP_MSEC):
                      ((WAIT_EAPP_SECS * 1000)/WAIT_SLEEP_MSEC);

  // wait till apps are launched
  for (i = 0; i < EAPPS_MAX; i++) {
    while (_eapp_pid[i] != 0 && count++ < max) {
      usleep(WAIT_SLEEP_MSEC*1000);
      waitpid(-1, &status, WNOHANG);
      snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", _eapp_pid[i]);
      memset(comm, 0x00, sizeof(comm));
      fd = open(comm_path, O_RDONLY);
      if (fd > 0) {
        ret = read(fd, comm, sizeof(comm) - 1);
        if (ret > 0) {
          if (strncmp(comm, EARLY_DFL_APP, sizeof(EARLY_DFL_APP)-1)) {
            _eapp_pid[i] = 0;
          }
        }
        close(fd);
      } else {
        // child process exited
        _eapp_pid[i] = 0;
      }
#ifdef EARLYINIT_DEBUG
      LOG(INFO) << "ES: eapp Path " << comm_path << " eappid id " << i;
#endif
    }
  }
  LOG(INFO) << "ES : eapps wait done";

  return (i < EAPPS_MAX)?0:-1;
}

int early_init_kmod(const char *idx)
{
  unsigned int i;
  int flag;
  int num_threads;

  android::earlyinit::InitKernelLogging(NULL);
#ifdef EARLYINIT_DEBUG
  LOG(INFO) << "ES: Init kernel Module " << idx;
#endif

  i = idx[0] - '0';
  if (i < sizeof(_eapp_info)/sizeof(_eapp_info[0]) &&
      _eapp_info[i].name[0] != 0) {
    char str[SHORT_STRING_MAX];
    if (!strncmp(_eapp_info[i].name, EMOD_END, sizeof(_eapp_info[i].name)-1))
      flag = LMP_MODPROBE;
    else if (strncmp(_eapp_info[i].name, ECHIME_APP, sizeof(_eapp_info[i].name)-1)
            && strncmp(_eapp_info[i].name, EAUDIO_APP, sizeof(_eapp_info[i].name)-1))
      flag = LMP_DIRECT;
    else
      flag = LMP_DIRECT_CHK_AUD;

    num_threads = bc_get_lmp()?std::thread::hardware_concurrency():1;

    snprintf(str, SHORT_STRING_MAX, "%s%s", ES_VMOD_PATH, _eapp_info[i].kfile);
    load_modules_parallel(str, ES_DFLMOD_PATH, num_threads,
        _eapp_info[i].tag, flag);
     return 0;
  }
  LOG(WARNING) << "ES : Init Kernel Mod failed for app idx " << i;

  return -1;
}

static pid_t __attribute__((unused)) fork_wait_for_child(int type, int run_if_fork_fail)
{
  pid_t pid = -1;
  int pipe_fd[2];
  int rdata;
  bool pipe_created;

  pipe_created = (pipe(pipe_fd) != -1)?true:false;
  if (!pipe_created) {
    LOG(WARNING) << "ES : pipe_fd failed, type " << type;
    printf("ES : pipe_fd failed, type %d", type);
  }
  // Even if pipe fails, create child process
  pid = fork();
  if (pid < 0 && run_if_fork_fail <= 0) {
    return -1;
  }

  if (pid <= 0) {
    int wdata = 1;

    // if fork fails sequetial execution
    if (pid == 0) {
      // Parent kill may cause child to exit, so ignore it.
      signal(SIGTERM, SIG_IGN);
      if (pipe_created) {
        close(pipe_fd[PIPE_RD]);
        write(pipe_fd[PIPE_WR], &wdata, sizeof(wdata));
        close(pipe_fd[PIPE_WR]);
      }
    }

    switch (type) {
      case ES_CTYPE_FW: {
        android::earlyinit::InitKernelLogging(NULL);
        prepare_fw_dir(true);
        break;
      }
      case ES_CTYPE_DEF2_MOD: {
        bool load_parallel = bc_get_lmp();
        load_modules_parallel(ES_DFLMOD_ORDER_2, ES_DFLMOD_PATH,
          load_parallel?std::thread::hardware_concurrency():1,
          EMOD_DEF_TAG_2, LMP_MODPROBE);
        break;
      }
      case ES_CTYPE_DI_MOD: {
        bool load_parallel = bc_get_lmp();
        load_modules_parallel(ES_DFLMOD_ORDER_DI, ES_DFLMOD_PATH,
          load_parallel?std::thread::hardware_concurrency():1,
          EMOD_DI_TAG, LMP_MODPROBE);
        break;
      }
      case ES_CTYPE_LOAD_SE: {
        load_precompiled_sepolicy();
        break;
      }
      default:
      break;
    }
    if (pid == 0) {
      _exit(0);
    }
  }
  if (pipe_created) {
    close(pipe_fd[PIPE_WR]);
    if (pid > 0) {
      read(pipe_fd[PIPE_RD], &rdata, sizeof(rdata)); // wait for child to start
    }
    close(pipe_fd[PIPE_RD]);
  }

  return pid;
}

int early_init(int init)
{
  int ret;
  bool log_fsinit = false;

  clearenv();
  setenv("PATH", DEFAULT_PATH, 1);

  _use_min_wait = !kcmd_bc_console_enabled();
  // Enable ES logging if console enabled or ES second stage
#ifdef __ANDROID_U__
  log_fsinit = true;
#endif

  // set kernel logging in perf build at stage II
  if (log_fsinit || (!_use_min_wait || init )) {
    android::earlyinit::InitKernelLogging(NULL);
  }

  boot_clock::time_point module_start_time = boot_clock::now();
  LOG(INFO) << "ES : Logging enabled at early-services, init " << init;

  // Get default cmdline and bootconfig values.
  _audio_reach = bc_get_ar();
  bc_boot_slot(_boot_slot);
  std::string tmp;
  android::earlyinit::get_kernel_module_param(tmp, tmp, _module_params, true);
  if (init) {
    ret = mount("/vendor_early_services", "/vendor_early_services", NULL,
                MS_BIND | MS_REC, NULL);
    if (ret < 0) {
      mknod("/dev/sedone", S_IFREG | 0400, makedev(0,0));
      LOG(WARNING) << "ES : mount failed! " << "errno " << errno;
      return -1;
    }

    mount("sysfs", "/sys", "sysfs", 0, NULL);

    // Reminder, Android host will crash if mount devtmpfs to /dev once early-init exit.
    //prepare_dir((char*)"dev");
    prepare_dir((char*)"procfs");
    prepare_dir((char*)"shm");
    prepare_dir((char*)"cgroup2");
    mkdirs("/dev/socket/agm", 0775);
    mkdirs("/dev/socket/camera", 0775);

    /* Create ais_server/qcxserver socket dir and camera data dir */

#if defined( __ANDROID_U__)
     load_default_modules();
     prepare_fw_dir(true);
     load_precompiled_sepolicy();
#else
    bool load_parallel = bc_get_lmp();
    pid_t pid_se;


    load_modules_parallel(ES_DFLMOD_ORDER_1, ES_DFLMOD_PATH,
         load_parallel?std::thread::hardware_concurrency():1,
         EMOD_DEF_TAG_1, LMP_MODPROBE);

    // Check for driver storage enumerations
    fork_wait_for_child(ES_CTYPE_FW, 1);
#ifndef PLATFORM_GEN4
    // Load second set of def-modules in parallel
    pid_t pid_def2 = fork_wait_for_child(ES_CTYPE_DEF2_MOD, 1);
#endif
    // Load Display modules in parallel
    fork_wait_for_child(ES_CTYPE_DI_MOD, 1);
    // Load sepolicies in parallel
    pid_se = fork_wait_for_child(ES_CTYPE_LOAD_SE, 1);

    // wait for sepol loading and def2 modules as its required for next steps.
    int max = (_use_min_wait)?((WAIT_PID_MIN_MSECS * 1000) / WAIT_SLEEP_USECS):
                      ((WAIT_PID_MAX_MSECS * 1000) / WAIT_SLEEP_USECS);
    wait_for_pid(pid_se, WAIT_SLEEP_USECS, max);
#ifndef PLATFORM_GEN4
    wait_for_pid(pid_def2, WAIT_SLEEP_USECS, max);
#endif

#endif // ! __ANDROID_U__

    selinux_android_restorecon("/vendor_early_services/early_services_init", 0);
    if (selinux_android_restorecon("/vendor_early_services/",
      SELINUX_ANDROID_RESTORECON_RECURSE) == -1) {
      LOG(WARNING) << "restorecon /vendor_early_services not success";
    }

    selabel_handle* sehandle = nullptr;
    sehandle = selinux_android_file_context_handle();
    selinux_android_set_sehandle(sehandle);

    char str[SHORT_STRING_MAX] = {0};
    auto module_elapse_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                boot_clock::now() - module_start_time);
    snprintf(str, SHORT_STRING_MAX, "M - ES early-init-fs-exit %dms",
          (int)module_elapse_time.count());

    write_marker(str);

    setexeccon("u:r:init:s0");
    const char *path = "/vendor_early_services/bin/early_services_init";
    const char *args[] = { path, "selinux", NULL };
    execv(path, const_cast<char**>(args));
    LOG(WARNING) << "ES : Exec for early init failed!!!";

    return 0;
  } // init flag

  LOG(WARNING) << "ES : Config Audio Reach: " << _audio_reach;
  // exit status of child will be in wait_for_early_apps()
  if (fork() == 0) {
    signal(SIGTERM, SIG_IGN);
    android::earlyinit::InitKernelLogging(NULL);
    prepare_fw_dir(false);
    usleep(50*000);
    exit(0);
  }

  getSysInfo("/sys/devices/soc0/soc_id", chipId);
  getSysInfo("/sys/devices/soc0/platform_subtype_id", platformId);
  set_permissions("/dev/null", 0666, AID_ROOT, AID_ROOT, "u:object_r:null_device:s0");
  set_permissions("/dev/urandom", 0666, AID_ROOT, AID_ROOT, "u:object_r:random_device:s0");
  load_kmod_and_nodes(EMOD_END);
  check_and_create_linker64();
  check_and_create_vendor_etc();
  check_and_create_vendor_firmware();
#ifdef __ANDROID_U__
  launch_test_app();
  launch_early_apps();
#else
  launch_early_apps();
#endif
  // wait for app exec
  wait_for_early_apps();

  mknod("/dev/sedone", S_IFREG | 0400, makedev(0,0));

  char str[SHORT_STRING_MAX] = {0};
  auto module_elapse_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                boot_clock::now() - module_start_time);
  snprintf(str, SHORT_STRING_MAX, "M - ES early-init-exit %dms",
          (int)module_elapse_time.count());

  write_marker(str);

  LOG(INFO) << "ES Loading Apps done";

  sleep(5);

  return 0;
}

int main(int argc, char* argv[])
{
  int init = 0;
  if (argc < 2) {
    LOG(ERROR) << "ES started without args!";
    return -1;
  }

  if (!strcmp(argv[1], "early_service")) {
    LOG(INFO) << "ES Init";
    init++;
    early_init(init);
  } else if (!strcmp(argv[1], "selinux")) {
    LOG(INFO) << "ES Init with Load Apps";
    early_init(init);
  } else if (!strcmp(argv[1], "mmmod") && argc > 2) {
    LOG(INFO) << "ES Init with load mmmod";
    early_init_kmod(argv[2]);
  }

  return 0;
}
