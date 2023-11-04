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
* Changes from Qualcomm Innovation Center are provided under the following license:
* Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#define END_TAG                 "<end>"
#define LINE_MAX                2048
#define SHORT_STRING_MAX        128
#define WHITESPACE              " \t\n\r"
#define KPI_VALUE_PATH          "/sys/kernel/boot_kpi/kpi_values"
#define GPIO_EXPORT             "/sys/class/gpio/export"
#define DRM_CARD_PATH           "/dev/dri/card0"
#define DRM_CARD2_PATH          "/dev/dri/card2"
#define VIDEO_CARD_PATH         "/dev/video32"
#define AUDIO_FW_PATH           "/vendor_early_services/vendor/firmware_mnt"
#define AUDIO_ADSP_FW_PATH      "vendor_early_services/vendor/firmware_mnt/image/adsp.mdt"
#define SMACK_LABEL_PATH        "/proc/self/attr/current"
#define SMACK_LABEL             "System"
#define DEFAULT_PATH            "/sbin:/usr/sbin:/bin:/usr/bin:/system/sbin:/system/bin:/system/xbin:/odm/bin:/vendor/bin:/vendor/xbin:vendor_early_services/sbin:vendor_early_services/system/sbin:vendor_early_services/system/bin:vendor_early_services/system/xbin:vendor_early_services/odm/bin:vendor_early_services/vendor/bin:vendor_early_services/vendor/xbin"

#define EARLY_SERVICES_SEPOL   "/vendor_early_services/vendor/etc/selinux/precompiled_sepolicy"
#define EARLY_DFL_APP          "early_services"
#define ECHIME_APP             "early_chime_Disabled"
#define ECHIME_APP_TMP         "early_chime"
#define ESPLASH_APP            "esplash"
#define EVIDEO_APP             "earlyVideo"
#define EAIS_APP               "ais_server"
#define ERVC_APP               "qcarcam_edrm_rvc"
#define EMOD_END               "emod_end"
#define PD_MAPPER_APP          "pd-mapper"

#define EMOD_TAG               "def"
#define EMOD_END_TAG           "def-end"
#define ECHIME_TAG             "audio"
#define ESPLASH_TAG            "splash"
#define EVIDEO_TAG             "video"
#define EAIS_TAG               "ais"
#define ERVC_TAG               "rvc"
#define PD_MAPPER_TAG          "pd-mapper-tag"


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
#define NUM_MODULE 32
#define ADSP_LOADER_KO "/vendor_early_services/vendor/lib/modules/adsp_loader_dlkm_legacy.ko"
#define DRM_CARD3_PATH          "/dev/dri/card3"
#define DRM_CARD4_PATH          "/dev/dri/card4"
#define AUDIO_CTRL_PATH         "/dev/snd/pcmC0D50p"
#define CAMERA_MDEV_PATH        "/dev/media0"
#define CAMERA_VDEV_PATH        "/dev/video0"
#define CAMERA_V4L_DEV_PATH     "/dev/v4l-subdev0"
#define DMA_HEAP_DIR            "/dev/dma_heap"
#ifdef __ANDROID_U__
#define CAMERA_DMA_HEAP_PATH    "/dev/dma_heap/qcom,system"
#else
#define CAMERA_DMA_HEAP_PATH    "/dev/dma_heap/qcom,display"
#endif

#define VIDEO_SYS_DMA_HEAP_PATH "/dev/dma_heap/qcom,system"

#define WAIT_SET_PERM_COUNT 4
#define WAIT_SET_PERM_SECS  15
#define WAIT_SET_PERM_MSECS 300
#define WAIT_EAPP_SECS      20
#define WAIT_EAPP_MSECS     2500
#define WAIT_SLEEP_MSEC     20
#define EAPPS_MAX           12

#define MM_DEPMOD_ORDER "/vendor_early_services/vendor/lib/modules/modules.order"
#define MM_DEPMOD_ORDER_END "/vendor_early_services/vendor/lib/modules/modules_end.order"
#define MM_DEPMOD_PATH  "/lib/modules/"
#define MM_MOD_ORDER_DI "/vendor_early_services/vendor/lib/modules/modules_di.order"
#define MM_MOD_ORDER_VI "/vendor_early_services/vendor/lib/modules/modules_vi.order"
#define MM_MOD_ORDER_AIS "/vendor_early_services/vendor/lib/modules/modules_ais.order"
#define MM_MOD_ORDER_RV "/vendor_early_services/vendor/lib/modules/modules_rv.order"
#define MM_MOD_ORDER_AU "/vendor_early_services/vendor/lib/modules/modules_au.order"
#define MM_R_MOD_ORDER_AU "/vendor_early_services/vendor/lib/modules/modules_r_au.order"
#define MM_MOD_PATH     "/vendor_early_services/vendor/lib/modules/"

#if defined(__ANDROID_U__) || defined(PLATFORM_GEN4)
#define SELINUXMNT "/sys/fs/selinux"

#define TEST_APP "init_early_test"
#define TEST_APP_CMD  "/vendor_early_services/system/bin/init_early_test"
#define TEST_APP_ENV "/vendor_early_services:/vendor_early_services/system:/vendor_early_services/system/lib64:/vendor_early_services/system/bin/bootstrap"
#define TEST_APP_PID "/vendor_early_services/run/early/init_early_test.pid"
#define TEST_APP_LOG "/vendor_early_services/run/init_early_test.txt"
#endif //__ANDROID_U__ || PLATFORM_GEN4

static pid_t eapp_pid[EAPPS_MAX];

static inline bool is_empty_line(const char* p);
static inline char *strstrip(char *s);
static inline int parse_line(char* p);
static void set_permissions(char *path, int permissions, int user, int group, char *context);
static void launch_early_apps(void);
static void set_video_permission(void);
static void set_video1_permission(void);
static int load_kmod_and_nodes(const char* mod_group);

bool bc_get_ar();

enum EnforcingStatus { SELINUX_PERMISSIVE, SELINUX_ENFORCING };

char   chipId[32]  = { 0 };
char   platformId[32]  = { 0 };

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
} app_launcher;

#define BIT_SET(p,n) ((p) & (1 << (n)))
#define uid_is_valid(uid) ((uid != (uid_t) UINT32_C(0xFFFFFFFF)) && \
            (uid != (uid_t) UINT32_C(0xFFFF)))
#define gid_is_valid(gid)  uid_is_valid(gid)

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

/*
 * Only support abs path
 */
static inline void mkdirs(const char* p, mode_t mode)
{
  char str[1024] = {0};
  struct stat st = {0};
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
  struct stat st = {0};
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
    default:
      printf("warning unknown input string %s for prepare_dir", p);
  }
  return;
}

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
  app_launcher.usleep = -1;

  for (i = 0; i < app_launcher.argv_used; i++)
    safe_free(&app_launcher.argv[i]);

  for (i = 1; i < app_launcher.env_used; i++)
    safe_free(&app_launcher.env[i]);

  app_launcher.argv_used = 0;
  app_launcher.env_used = 0;
  app_launcher.bindcpumask = -1;
  app_launcher.priority = -1;
  app_launcher.env[app_launcher.env_used++] = DEFAULT_PATH; //set DEFAULT_PATH as static env[0] path for all ES app's

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
    case '<':/* end */
      /*
       * When comes to the end, start up the app
       */
      if (strncmp(p, END_TAG, strlen(END_TAG)))
        goto out;

      if (!strncmp(app_launcher.appname, ECHIME_APP_TMP, strlen(ECHIME_APP)) &&
          bc_get_ar()) {
        LOG(INFO) << "ES : Not Launching app " << app_launcher.appname;
        goto out;
      }

      pid = fork();
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

        // load kmod, if applicable for early app
        load_kmod_and_nodes(app_launcher.appname);

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

        app_launcher.env[app_launcher.env_used] = "LD_LIBRARY_PATH=/vendor_early_services/system/lib64";
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
            return -1;
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
        _exit(0);
      }

      // wait for initial display before other apps launch
      if (!strncmp(app_launcher.appname, ESPLASH_APP, strlen(ESPLASH_APP))) {
        while(access(DRM_CARD3_PATH, F_OK) == -1) usleep(5*1000);
      }

      printf("fire up %s \r\n", app_launcher.appname);
      break;
    default:
      printf("unknown config line %s\r\n", p);
  }

out:
  return pid;
}
/*
 * Check if line is empty or not
 */
static inline bool is_empty_line(const char* p)
{
  return (strspn(p, WHITESPACE) == strlen(p));
}
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

bool bc_get_lmp() {
  bool load_parallel = false;
  android::earlyinit::import_kernel_bootconfig(false,
     [&](const std::string& key, const std::string& value, bool in_qemu) {
    (void)in_qemu;
    if (key == "androidboot.load_modules_parallel" && value == "\"true\"") {
      load_parallel = true;
#ifdef __ANDROID_U__
      load_parallel = false;
#endif
    }
  });
  // LOG(INFO) << "ES : Config Modules Parallel load: " << load_parallel;
  return load_parallel;
}

bool bc_get_ar() {
  bool audio_reach = false;
  android::earlyinit::import_kernel_bootconfig(false,
     [&](const std::string& key, const std::string& value, bool in_qemu) {
    (void)in_qemu;
    if (key == "androidboot.audio" && value == "\"audioreach\"") {
      audio_reach = true;
    }
  });
  LOG(WARNING) << "ES : Config Audio Reach: " << audio_reach;
  return audio_reach;
}

EnforcingStatus bc_get_se() {
  EnforcingStatus status = SELINUX_ENFORCING;
  android::earlyinit::import_kernel_bootconfig(false,
    [&](const std::string& key, const std::string& value, bool in_qemu) {
    (void)in_qemu;
    if (key == "androidboot.selinux" && value == "\"permissive\"") {
      status = SELINUX_PERMISSIVE;
    }
  });
  LOG(INFO) << "ES : Selinux mode: " << status;
  return status;
}

bool IsEnforcing() {
  return bc_get_se() == SELINUX_ENFORCING;
}

void set_permissions(char *path, int permissions, int user, int group, char *context){
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

int getSysInfo(char * fileName, char * strName) {
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

static int wait_for_file(const char* file, int sleep_msec, int count)
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

  if (ret < 0) {
    LOG(INFO) << "ES File '" << file << "' not found!";
  } else {
    LOG(INFO) << "ES File '" << file << "' found!";
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


#ifdef __ANDROID_U__
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
#else
  if (!dma_heap_device_created) {
    if (access("/sys/class/dma_heap/qcom,display/uevent", F_OK) == 0) {
      if(get_device_major_minor("/sys/class/dma_heap/qcom,display/uevent", &major, &minor))
      {
        mkdir(DMA_HEAP_DIR, 0666);
        mknod(CAMERA_DMA_HEAP_PATH, S_IFCHR | 0666,
            makedev(major, minor));

        set_permissions(DMA_HEAP_DIR, 0755, AID_ROOT,
            AID_ROOT, "u:object_r:dmabuf_heap_device:s0");
        set_permissions(CAMERA_DMA_HEAP_PATH, 0666, AID_SYSTEM,
            AID_SYSTEM, "u:object_r:vendor_dmabuf_display_heap_device:s0");
        dma_heap_device_created = 1;
        LOG(INFO) << "ES camera dma_heap device nodes ready";
        write_marker("M - EarlyInit dma heap nodes ready");
      }
    }
  }
#endif
 
  return dma_heap_device_created;
}

static int check_camera_card2_ready(void)
{
  //camera
  static int card2_device_created = 0;
  int major = 0, minor = 0;

  if (!card2_device_created) {
    if (access("/sys/class/drm/card2/uevent", F_OK) == 0) {
      if(get_device_major_minor("/sys/class/drm/card2/uevent", &major, &minor))
      {
        mkdir("/dev/dri", 0666);
        mknod(DRM_CARD2_PATH, S_IFCHR | 0666,
            makedev(major, minor));

        set_permissions(DRM_CARD2_PATH, 0666, AID_ROOT,
            AID_GRAPHICS, "u:object_r:graphics_device:s0");
        card2_device_created = 1;
        LOG(INFO) << "ES camera card2 device nodes ready";
        write_marker("M - EarlyInit card2 nodes ready");
      }
    }
  }

  return card2_device_created;
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
      rvc_device_created = 1;
      LOG(INFO) << "ES rvc device nodes ready";
      write_marker("M - EarlyInit rvc nodes ready");
    }
  }

  return rvc_device_created;
}

#define DRM_CARD4_DIR        "/dev/dri"
static int check_video_device_ready(void)
{
  //video
  static int video_device_created = 0;
  int major = 0, minor = 0;

  if (!video_device_created) {
    if ((access("/sys/class/drm/card4/uevent", F_OK) == 0) &&
        (access("/sys/class/dma_heap/qcom,system/uevent", F_OK) == 0) &&
        (access("/sys/class/video4linux/video32/uevent", F_OK) == 0)) {
      if (get_device_major_minor("/sys/class/drm/card4/uevent", &major, &minor))
      {
        mkdir(DRM_CARD4_DIR, 0666);
        mknod(DRM_CARD4_PATH, S_IFCHR | 0666,
            makedev(major, minor));
      }
      if (get_device_major_minor("/sys/class/dma_heap/qcom,system/uevent", &major, &minor))
      {
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
      set_video_permission();
      set_video1_permission();
      LOG(INFO) << "ES video device nodes ready";
      write_marker("M - EarlyInit video nodes ready");
      video_device_created = 1;
    }
  }

  return video_device_created;
}

#define DRM_CARD3_DIR        "/dev/dri"
static int check_esplash_device_ready(void)
{
  //esplash
  static int esplash_device_created = 0;
  int major = 0, minor = 0;

  if (!esplash_device_created) {
    if (access("/sys/class/drm/card3/uevent", F_OK) == 0) {
      if(get_device_major_minor("/sys/class/drm/card3/uevent", &major, &minor))
      {
        mkdir(DRM_CARD3_DIR, 0666);
        mknod(DRM_CARD3_PATH, S_IFCHR | 0666,
            makedev(major, minor));

        LOG(INFO) << "ES esplash device nodes ready";
        write_marker("M - EarlyInit esplash nodes ready");
        esplash_device_created = 1;
      }
    }
  }
  return esplash_device_created;
}

static int check_storage_device_ready(void)
{
  static int sto_device_created = 0;

  if (!sto_device_created) {
    if (access("/sys/block/sda/uevent", F_OK) == 0 ||
        access("/sys/block/sde/uevent", F_OK) == 0) {
      LOG(INFO) << "ES SD nodes ready";
      write_marker("M - EarlyInit SD nodes ready");
      sto_device_created = 1;
    }
  }

  return sto_device_created;
}

static void set_audio_permission(void)
{
  LOG(INFO) << "ES : Set Audio Permissions";
  set_permissions("/dev/snd", 00777, AID_ROOT, AID_ROOT, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/controlC0", 00666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D49c", 00666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D48p", 00666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D50p", 00666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D53c", 00666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D55p", 00666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/pcmC0D48p", 00666, AID_SYSTEM, AID_AUDIO, "u:object_r:audio_device:s0");

  return;
}

static void set_splash_permission(void)
{

  LOG(INFO) << "ES : Set Splash Permissions";
  set_permissions(DRM_CARD3_DIR, 0755, AID_ROOT, AID_ROOT, "u:object_r:device:s0");
  set_permissions(DRM_CARD3_PATH, 0666, AID_ROOT, AID_GRAPHICS, "u:object_r:graphics_device:s0");
  return;
}

// set permissions for video resources
static void set_video_permission(void)
{
  set_permissions(DRM_CARD4_PATH, 0666, AID_ROOT, AID_GRAPHICS, "u:object_r:graphics_device:s0");
  LOG(INFO) << "EarlyVideo Setting permission to dri card completed";
  return;
}

static void set_video1_permission(void)
{
  set_permissions(VIDEO_CARD_PATH, 0666, AID_ROOT, AID_GRAPHICS, "u:object_r:video_device:s0");
  LOG(INFO) << "EarlyVideo Setting permission to video device completed";
  return;
}

static void set_camera_permission(void)
{
  LOG(INFO) << "ES : Set Camera Permissionsi for mdev";
  set_permissions("/dev/media0", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/media1", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  LOG(INFO) << "ES : Set Camera Permissions Completed for mdev";
  return;
}

static void set_camera_permission1(void)
{
  LOG(INFO) << "ES : Set Camera Permissions1 for vdev";
  set_permissions("/dev/video0", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/video1", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  LOG(INFO) << "ES : Set Camera Permissions1 Completed for vdev";
  return;
}

static void set_camera_permission2(void)
{
  LOG(INFO) << "ES : Set Camera Permissions2 for v4l-subdev";
  set_permissions("/dev/v4l-subdev1", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev2", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev3", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev4", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev5", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev6", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev7", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev8", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev9", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev10", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/socket/camera", 0775, AID_ROOT, AID_CAMERA, "u:object_r:vendor_camera_socket:s0");
  selinux_android_restorecon("/dev/socket/camera", SELINUX_ANDROID_RESTORECON_RECURSE);
  set_permissions("/dev/v4l-subdev11", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev12", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev13", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev14", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev15", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev16", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/dev/v4l-subdev0", 0666, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  LOG(INFO) << "ES : Set Camera Permissions2 Completed for v4l-subdev";

  return;
}

static int prepare_fw_dir(bool set_km)
{
  int i, len;
  std::string modemTmpStr;
  std::string modemStr("/dev/block");
  const char* mnt = NULL;
  unsigned int count = 0, max = (WAIT_SET_PERM_SECS * 1000) / WAIT_SLEEP_MSEC;
  boot_clock::time_point module_start_time = boot_clock::now();

  android::earlyinit::import_kernel_cmdline(false,
        [&](const std::string& key, const std::string& value, bool in_qemu) {
    (void)in_qemu;
    if (key == "modem") {
      modemTmpStr = value;
    }
    if (key == "buildvariant" && value == "user") {
      max = (WAIT_SET_PERM_MSECS) / WAIT_SLEEP_MSEC;
    }
  });

  if (set_km) {
    while (count++ < max) {
      if (check_storage_device_ready()) break;
      usleep(WAIT_SLEEP_MSEC * 1000);
    }
    // Enumerate dev nodes - fw
    mknod("/dev/kmdone", S_IFREG | 0400, makedev(0,0));

    return 0;
  }

  if (access(AUDIO_FW_PATH, F_OK) == -1) {
    LOG(WARNING) << "ES : AUDIO_FW_PATH doesn't exist";
    mkdirs(AUDIO_FW_PATH, 0755);
  }

  // use file name to attach to path /dev/block
  len = modemTmpStr.length();
  if (len) {
    const char *p = modemTmpStr.c_str();
    const char *p1 = p;
    for (i = 0;  i < len; i++, p++) {
      if (*p == '/') p1 = p;
    }
    modemStr += p1;
  }

  if (len && wait_for_file(modemStr.c_str(), 30, 50) == 0)
    mnt = modemStr.c_str();
  if (!mnt && len && wait_for_file(modemTmpStr.c_str(), 10, 10) == 0)
    mnt = modemTmpStr.c_str();

  if (mnt) {
    if (mount(mnt, AUDIO_FW_PATH, "vfat", MS_RDONLY, "context=u:object_r:firmware_file:s0") < 0) {
      LOG(WARNING) << "ES : modemstr mount failed, err " << errno;
    } else {
      LOG(INFO) << "ES : modemstr mount success.";
    }
  } else {
    LOG(WARNING) << "ES : modemstr Not Found!";
  }
  char str[SHORT_STRING_MAX] = {0};
  auto module_elapse_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                boot_clock::now() - module_start_time);
  snprintf(str, SHORT_STRING_MAX, "%s%d%s", "M - ES fw-load took ",
           (int)module_elapse_time.count(), "ms");
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
  // Load the vendor early service policy
  std::string precompiled_sepolicy_file = EARLY_SERVICES_SEPOL;
  write_marker("M - EarlyInit SEPolicyLoad Start");
  int fd1 = open(precompiled_sepolicy_file.c_str(),
                 O_RDONLY | O_CLOEXEC | O_BINARY, 0775);
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
      LOG(INFO) << "ES : Successfully loaded precompiled sepolicy file: "
                << precompiled_sepolicy_file.c_str();
    }
    close(fd1);
  }
  write_marker("M - EarlyInit SEPolicyLoad End");

  return 0;
}

static int load_kmod_and_nodes(const char* appname)
{
  pid_t pid;
  int wstatus;
  pid_t wpid;
  unsigned int count = 0, max = (WAIT_SET_PERM_SECS * 1000)/WAIT_SLEEP_MSEC;
  int set_count = 1;
  const char* tag;
  bool no_dev = false;

  void (*set_perm[WAIT_SET_PERM_COUNT])(void) = {0};
  int (*check_dev[WAIT_SET_PERM_COUNT])(void) = {0};
  char *dev_path[WAIT_SET_PERM_COUNT] = {0};

  // Add null check for appname to fix kw issue.
  if (appname == NULL) {
    LOG(INFO) << "ES : appname NULL pointer\n ";
    return -1;
  }

  // Every set_perm must set dev_path. check_dev is independent.
  if (!strncmp(appname, ESPLASH_APP, strlen(ESPLASH_APP))) {
    check_dev[0] = check_esplash_device_ready;
    set_perm[0] = set_splash_permission;
    dev_path[0] = (char*)DRM_CARD3_PATH;
    tag = ESPLASH_TAG;
  } else if (!strncmp(appname, EVIDEO_APP, strlen(EVIDEO_APP))) {
    check_dev[0] = check_video_device_ready;
    set_perm[0] = set_video_permission;
    dev_path[0] = (char*)DRM_CARD4_PATH;
    tag = EVIDEO_TAG;
  } else if (!strncmp(appname, EAIS_APP, strlen(EAIS_APP))) {
    wait_for_file(DRM_CARD3_PATH, 30, 50);
    check_dev[0] = check_rvc_device_ready;
    set_perm[0] = set_camera_permission;
    dev_path[0] = (char*)CAMERA_MDEV_PATH;
    set_perm[1] = set_camera_permission1;
    dev_path[1] = (char*)CAMERA_VDEV_PATH;
    set_perm[2] = set_camera_permission2;
    dev_path[2] = (char*)CAMERA_V4L_DEV_PATH;
    tag = EAIS_TAG;
  } else if (!strncmp(appname, ERVC_APP, strlen(ERVC_APP))) {
    check_dev[0] = check_gfx_device_ready;
    check_dev[1] = check_camera_card2_ready;
    check_dev[2] = check_dma_heap_device_ready;
    tag = ERVC_TAG;
  } else if (!strncmp(appname, ECHIME_APP, strlen(ECHIME_APP))) {
    check_dev[0] = check_esplash_device_ready;
    set_perm[0] = set_audio_permission;
    dev_path[0] = (char*)AUDIO_CTRL_PATH;
    tag = ECHIME_TAG;
  } else if (!strncmp(appname, PD_MAPPER_APP, strlen(PD_MAPPER_APP))) {
    wait_for_file(AUDIO_ADSP_FW_PATH, 50, 500);
    tag = PD_MAPPER_TAG;
  } else if (!strncmp(appname, EMOD_END, strlen(EMOD_END))) {
    no_dev = true;
  } else {
    //LOG(INFO) << "ES : Can't load mod for this app " << appname;
    return -1;
  }

#ifdef EARLYINIT_DEBUG
  boot_clock::time_point module_start_time = boot_clock::now();
#endif

  if ((pid = fork()) == 0)  {
    LOG(INFO) << "ES: Fork for mmmod " << appname;
    setexeccon("u:r:vendor_init:s0");
    char *path = "/vendor_early_services/bin/early_services_init";
    char app[SHORT_STRING_MAX] = {0};
    if (appname)
      strlcpy(app, appname, sizeof(app));

    char *args[] = { path, "mmmod", app, NULL };
    execv(path, args);
    LOG(WARNING) << "ES : Exec for mmmod, failed!";
    _exit(0);
  }

  do {
    wpid = waitpid(pid, &wstatus, 0);
    if (wpid == -1 || wpid != 0) break;
  } while (wpid == 0);

  // If no dev nodes to check, just return
  if (no_dev)
    return 0;

  char str[SHORT_STRING_MAX] = {0};
#ifdef EARLYINIT_DEBUG
  auto module_elapse_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                boot_clock::now() - module_start_time);
  snprintf(str, SHORT_STRING_MAX, "%s%6s%s%d%s", "M - ES kmod-", tag,
           " took ", (int)module_elapse_time.count(), "ms");
  write_marker(str);
#else
  boot_clock::time_point module_start_time = boot_clock::now();
#endif

  // get the max value based on build variant
  android::earlyinit::import_kernel_cmdline(false,
        [&](const std::string& key, const std::string& value, bool in_qemu) {
    (void)in_qemu;
    if (key == "buildvariant" && value == "user") {
      max = (WAIT_SET_PERM_MSECS) / WAIT_SLEEP_MSEC;
    }
  });
  LOG(INFO) << "ES : wait and set perm " << tag << " iter max " << max;
  while (count++ < max && set_count) {
    set_count = 0;
    for (int i = 0; i < WAIT_SET_PERM_COUNT; i++) {
      if (check_dev[i]) {
        if (check_dev[i]())
          check_dev[i] = NULL;
        set_count++;
        // LOG(INFO) << "ES : wait and set check perm " << tag << i;
      }
      if (set_perm[i]) {
        if (access(dev_path[i], F_OK) == 0) {
          set_perm[i]();
          set_perm[i] = NULL;
        }
        set_count++;
        // LOG(INFO) << "ES : wait and set perm " << tag << i;
      }
    }

    usleep(WAIT_SLEEP_MSEC * 1000);
  }
  LOG(INFO) << "ES : wait and set perm time " << (count * WAIT_SLEEP_MSEC)/1000
            << "s app " << tag;

  auto module_elapse_time1 = std::chrono::duration_cast<std::chrono::milliseconds>(
                boot_clock::now() - module_start_time);
  snprintf(str, SHORT_STRING_MAX, "%s%s%s%d%s", "M - ES wait-set-perm-", tag,
           " took ", (int)module_elapse_time1.count(), "ms");
  write_marker(str);

  return 0;
}

static int load_modules_parallel(const std::string& fl,
                   const std::string& mod_path, const int th_count,
                   const std::string& logtag)
{
  boot_clock::time_point module_start_time = boot_clock::now();
  const int TH_MAX = th_count;
  const int TH_MIN = 3;
  std::string mlist;
  int load_count = 0;

  if (!android::base::ReadFileToString(fl, &mlist, false))
    return -1;

  // LOG(INFO) << "Loading modules " << mod_path << " file " << fl;
  std::vector<std::string> lines = android::base::Split(mlist, "\n");
  for (const std::string line : lines) {
    if (line.empty())
      continue;

    std::vector<std::thread> th_mods;
    std::mutex mods_lock;
    std::vector<std::string> m = android::base::Split(line, " ");
    if (m.empty())
      continue;

    int num_threads = (m.size() < (TH_MIN +1) && TH_MAX > TH_MIN)?TH_MIN:TH_MAX;
    auto mod_load_thread_fn = [&] {
      std::unique_lock lk(mods_lock);
      while (!m.empty()) {
        auto ml = std::move(m.back());
        m.pop_back();
        if (ml.empty())
          continue;
        load_count++;
        lk.unlock();
        std::string mn = mod_path;
        mn += ml;
        mn +=".ko";
        int fd = open(mn.c_str(), O_RDONLY);
        if (fd > 0) {
          std::string param;
          android::earlyinit::get_kernel_module_param(ml, param);
          int ret = finit_module(fd, param.c_str(), 0);
          if (ret < 0 && errno != EEXIST) {
            LOG(INFO) << "fd = " << fd << "ES : init_module failed " << mn << " errno: " << errno;
          } else {
            // LOG(INFO) << "ES : init_module success for: " << mn;
          }
          close(fd);
        } else {
          LOG(WARNING) << "ES : Failed to open module " << mn;
        }

        lk.lock();
      }
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
  snprintf(str, SHORT_STRING_MAX, "M - ES %s-mod took %d%s", logtag.c_str(),
          (int)module_elapse_time.count(), "ms");

  write_marker(str);

  LOG(INFO) << "ES : Load modules done, count " << load_count;

  return 0;
}

#if defined(__ANDROID_U__) || defined(PLATFORM_GEN4)
static void launch_test_app(void)
{
  int fd;
  size_t i = 0;
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

  app_launcher.env[app_launcher.env_used] = "LD_LIBRARY_PATH=/vendor_early_services/system/lib64";
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
        eapp_pid[i++] = pid;
      }
    }
    memset(buf, 0, sizeof(buf));
  }
  if (i == EAPPS_MAX)
    LOG(WARNING) << "ES : Max Apps limit reached!";
}

#if defined(__ANDROID_U__) || defined(PLATFORM_GEN4)
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
#endif // __ANDROID_U__ || PLATFORM_GEN4

int early_init_kmod(const char *appname)
{
  android::earlyinit::InitKernelLogging(NULL);
  std::string file, path, tag;

  // LOG(INFO) << "ES: Init kernel Module " << appname;
  if (!strncmp(appname, ESPLASH_APP, strlen(ESPLASH_APP))) {
    file = MM_MOD_ORDER_DI;
    path = MM_DEPMOD_PATH;
    tag = ESPLASH_TAG;
  } else if (!strncmp(appname, EVIDEO_APP, strlen(EVIDEO_APP))) {
    file = MM_MOD_ORDER_VI;
    path = MM_MOD_PATH;
    tag = EVIDEO_TAG;
  } else if (!strncmp(appname, EAIS_APP, strlen(EAIS_APP))) {
    file = MM_MOD_ORDER_AIS;
    path = MM_MOD_PATH;
    tag = EAIS_TAG;
  } else if (!strncmp(appname, ERVC_APP, strlen(ERVC_APP))) {
    file = MM_MOD_ORDER_RV;
    path = MM_MOD_PATH;
    tag = ERVC_TAG;
  } else if (!strncmp(appname, ECHIME_APP, strlen(ECHIME_APP))) {
    file = (bc_get_ar())?MM_R_MOD_ORDER_AU:MM_MOD_ORDER_AU;
    path = MM_MOD_PATH;
    tag = ECHIME_TAG;
  } else if (!strncmp(appname, PD_MAPPER_APP, strlen(PD_MAPPER_APP))) {
    if(bc_get_ar()) {
      file = MM_R_MOD_ORDER_AU;
      path = MM_MOD_PATH;
      tag = PD_MAPPER_TAG;
    }
  } else if (!strncmp(appname, EMOD_END, strlen(EMOD_END))) {
    file = MM_DEPMOD_ORDER_END;
    path = MM_DEPMOD_PATH;
    tag = EMOD_END_TAG;
  } else {
    return 0;
  }

  load_modules_parallel(file, path,
     bc_get_lmp()?std::thread::hardware_concurrency():1, tag);

  return 0;
}

int early_init(int init)
{
  int ret;

  clearenv();
  setenv("PATH", DEFAULT_PATH, 1);
  android::earlyinit::InitKernelLogging(NULL);
  LOG(INFO) << "ES : Logging enabled at early-services, init " << init;

  if (init) {
    ret = mount("/vendor_early_services", "/vendor_early_services", NULL,
                MS_BIND | MS_REC, NULL);
    if (ret < 0) {
      mknod("/dev/sedone", S_IFREG | 0400, makedev(0,0));
      LOG(WARNING) << "ES : mount failed! " << "errno " << errno;
      return -1;
    }

    mount("sysfs", "/sys", "sysfs", 0, NULL);
    prepare_dir("shm");

    /* Create ais_server socket dir and camera data dir */
    mkdir("/dev/socket", 0775);
    mkdir("/dev/socket/camera", 0775);
#if defined( __ANDROID_U__) || defined(PLATFORM_GEN4)
     load_default_modules();
#else
    load_modules_parallel(MM_DEPMOD_ORDER, MM_DEPMOD_PATH,
             bc_get_lmp()?std::thread::hardware_concurrency():1, EMOD_TAG);
#endif // __ANDROID_U__ || PLATFORM_GEN4
    if (fork() == 0) {
      signal(SIGTERM, SIG_IGN);
      prepare_fw_dir(true);
      _exit(0);
    }
    load_precompiled_sepolicy();

    selinux_android_restorecon("/vendor_early_services/early_services_init", 0);
    if (selinux_android_restorecon("/vendor_early_services/",
      SELINUX_ANDROID_RESTORECON_RECURSE) == -1) {
      LOG(WARNING) << "restorecon /vendor_early_services not success";
    }

    selabel_handle* sehandle = nullptr;
    sehandle = selinux_android_file_context_handle();
    selinux_android_set_sehandle(sehandle);
    setexeccon("u:r:init:s0");
    char *path = "/vendor_early_services/bin/early_services_init";
    char *args[] = { path, "selinux", NULL };
    execv(path, args);
    LOG(WARNING) << "ES : Exec for early init failed!!!";

    return 0;
  } // init flag

  prepare_fw_dir(false);
  getSysInfo("/sys/devices/soc0/soc_id", chipId);
  getSysInfo("/sys/devices/soc0/platform_subtype_id", platformId);
  set_permissions("/dev/null", 0666, AID_ROOT, AID_ROOT, "u:object_r:null_device:s0");
  set_permissions("/dev/urandom", 0666, AID_ROOT, AID_ROOT, "u:object_r:random_device:s0");

#ifdef __ANDROID_U__
  launch_test_app();
  launch_early_apps();
#elif PLATFORM_GEN4
  launch_test_app();
#else
  launch_early_apps();
#endif

  char comm[SHORT_STRING_MAX/2];
  char comm_path[SHORT_STRING_MAX/2];
  unsigned int count = 0, max = (WAIT_EAPP_SECS * 1000)/WAIT_SLEEP_MSEC;
  int i, fd;

  // get the max value based on build variant
  android::earlyinit::import_kernel_cmdline(false,
        [&](const std::string& key, const std::string& value, bool in_qemu) {
    (void)in_qemu;
    if (key == "buildvariant" && value == "user") {
      max = (WAIT_EAPP_MSECS)/WAIT_SLEEP_MSEC;
    }
  });

  // wait till apps are launched
  for (i = 0; i < EAPPS_MAX; i++) {
    while (eapp_pid[i] != 0 && count++ < max) {
      usleep(WAIT_SLEEP_MSEC*1000);
      snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", eapp_pid[i]);
      memset(comm, 0x00, sizeof(comm));
      fd = open(comm_path, O_RDONLY);
      if (fd > 0) {
        ret = read(fd, comm, sizeof(comm) - 1);
        if (ret > 0) {
          if (strncmp(comm, EARLY_DFL_APP, strlen(EARLY_DFL_APP))) {
            eapp_pid[i] = 0;
          }
        }
        close(fd);
      } else {
        // LOG(INFO) << "ES : open failed " << comm_path << " err " << errno ;
        // child process exited
        eapp_pid[i] = 0;
      }
    }
  }

  load_kmod_and_nodes(EMOD_END);

  mknod("/dev/sedone", S_IFREG | 0400, makedev(0,0));
  write_marker("M - early-init-exit");

  LOG(INFO) << "ES Loading Apps done";

  sleep(5);

  return 0;
}

int main(int argc, char* argv[])
{
  int init = 0;

  if (argc < 1) {
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
  } else if (!strcmp(argv[1], "mmmod")) {
    LOG(INFO) << "ES Init with load mmmod";
    early_init_kmod(argv[2]);
  }

  return 0;
}
