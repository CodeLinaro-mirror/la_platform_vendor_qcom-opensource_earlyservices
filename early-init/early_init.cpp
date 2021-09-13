/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
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

#define _GNU_SOURCE
//#define TEMP_SOLUTION
#define EARLYINIT_DEBUG

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
#include <private/android_filesystem_config.h>
#include <sys/sysinfo.h>

#ifdef EARLYINIT_DEBUG
#include <dirent.h>
#endif

#define DEFAULT_CONF            "/early_services/etc/early_init.conf"
#define END_TAG                 "<end>"
#define LINE_MAX                2048
#define WHITESPACE              " \t\n\r"
#define KPI_VALUE_PATH          "/sys/kernel/boot_kpi/kpi_values"
#define GPIO_EXPORT             "/sys/class/gpio/export"
#define DRM_CARD_PATH           "/dev/dri/card0"
#define VIDEO_CARD_PATH         "/dev/video32"
#define AUDIO_FW_PATH           "/vendor/firmware_mnt"
#define SMACK_LABEL_PATH        "/proc/self/attr/current"
#define SMACK_LABEL             "System"
#define DEFAULT_PATH    "/sbin:/usr/sbin:/bin:/usr/bin:/system/sbin:/system/bin:/system/xbin:/odm/bin:/vendor/bin:/vendor/xbin:early_services/sbin:early_services/system/sbin:early_services/system/bin:early_services/system/xbin:early_services/odm/bin:early_services/vendor/bin:early_services/vendor/xbin"


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
#include <cutils/android_reboot.h>
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

#define init_module(module_image, len, param_values) syscall(__NR_init_module, module_image, len, param_values)
#define finit_module(fd, param_values, flags) syscall(__NR_finit_module, fd, param_values, flags)
#define NUM_MODULE 10

char audio_modules[NUM_MODULE][64] = {
"/early_services/vendor/lib/modules/snd_event_dlkm.ko",
"/early_services/vendor/lib/modules/q6_notifier_dlkm.ko",
"/early_services/vendor/lib/modules/apr_dlkm.ko",
"/early_services/vendor/lib/modules/adsp_loader_dlkm.ko",
"/early_services/vendor/lib/modules/q6_dlkm.ko",
"/early_services/vendor/lib/modules/platform_dlkm.ko",
"/early_services/vendor/lib/modules/native_dlkm.ko",
"/early_services/vendor/lib/modules/stub_dlkm.ko",
"/early_services/vendor/lib/modules/hdmi_dlkm.ko",
"/early_services/vendor/lib/modules/machine_dlkm.ko"};

static inline bool is_empty_line(const char* p);
static inline char *strstrip(char *s);
static inline int parse_line(char* p);

enum EnforcingStatus { SELINUX_PERMISSIVE, SELINUX_ENFORCING };
enum BootStatus { NORMAL_BOOT, RECOVERY_BOOT };

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
  int fd = -1;

  fd = open(KPI_VALUE_PATH, O_WRONLY);
  if (fd > 0) {
    (void)write(fd, name, strlen(name));
  } else {
    printf("open bootkpi for name %s failed %s\r\n", name, strerror(errno));
  }
  safe_close(fd);

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
static inline void mkdirs(char* p, mode_t mode)
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
    case 'a':
      if (0 == strncmp(p + 1, "udio_fw", strlen("udio_fw"))) {
        /*
         * Mount audio firmware partition
         */
        if (stat(AUDIO_FW_PATH, &st) == -1) {
          perror("AUDIO_FW_PATH doesn't exist");
          mkdirs(AUDIO_FW_PATH, 0755);
        }

        /* TODO: Do not hard code dev node, sde4 is modem_a/adsp firmware  partition */
        ret = mount("/dev/sde4", AUDIO_FW_PATH, "vfat", MS_RDONLY, NULL);
        if (ret < 0) {
          perror("mount /dev/sde4 failed");
        }
      }
      break;
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
        if (stat("/dev", &st) == -1) {
          perror("/dev folder doesn't exist");
          mkdir("/dev", 0755);
        }
        ret = mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
        if (ret < 0) {
          perror("mount devtmpfs failed");
        }
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
  // Should set group first
  printf("gid is %d", pw->pw_gid);
  if (!gid_is_valid(pw->pw_gid)) {
    perror("gid is not valid\r\n");
  }
  if (0 != setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid)) {
    perror("setresgid failed\r\n");
  }

  printf("uid is %d", pw->pw_uid);
  if (!uid_is_valid(pw->pw_uid)) {
    perror("uid is not valid\r\n");
  }
  if (0 != setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid)) {
    perror("setresuid failed\r\n");
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
static inline int parse_line(char* p)
{
  size_t i = 0;
  char* t;
  pid_t pid;
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

      pid = fork();
      if (pid < 0) {
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
          if (0 != sched_setscheduler( 0, SCHED_FIFO, &sp))
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

        app_launcher.env[app_launcher.env_used] = "LD_LIBRARY_PATH=/early_services/system/lib64";
        app_launcher.env_used++;
        app_launcher.argv[app_launcher.argv_used] = NULL;
        app_launcher.env[app_launcher.env_used] = NULL;

      //  write_smack_label(SMACK_LABEL);

        if (app_launcher.username) {
          enforce_user(app_launcher.username);
        }
        memset(marker, 0, 50);
        snprintf(marker, 49 ,"M - Launch %s app", app_launcher.appname);
        write_marker(marker);

        if (app_launcher.cmd) {
          ret = execvpe(app_launcher.cmd,app_launcher.argv,app_launcher.env);
          if(ret < 0) {
            printf("App launch failed %s \r\n", app_launcher.appname);
            memset(marker, 0, 50);
            snprintf(marker, 49 ,"M - Launch %s app failed %d", app_launcher.appname, errno);
            write_marker(marker);
          }
        }
        exit(0);
      }

      printf("fire up %s \r\n", app_launcher.appname);
      break;
    default:
      printf("unknown config line %s\r\n", p);
  }

out:
  return 0;
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

static void insert_audio_modules(void)
{
  struct stat st = {0};
  struct stat st_mod = {0};
  struct timeval tv;
  char marker_time[64];
  int fd = -1;
  int sret = 0, eret = 0;
  size_t image_size;
  static char marker[50];
  int i, ret = 0;

    /* Load Audio modules */
    memset(marker, 0, 50);
    snprintf(marker, 49 ,"M - Insert Audio modules - Start");
    write_marker(marker);

    // Insert Modules using init_module()
    for(i = 0; i < NUM_MODULE; i++ ) {
    memset(marker, 0, 50);
    snprintf(marker, 49 ,"M - Inserting %d %s",i,&audio_modules[i][32]);
    write_marker(marker);
        fd = open(audio_modules[i], O_RDONLY);
        if (finit_module(fd, "", 0) != 0) {
            freopen("/early_services/dev/kmsg", "w", stdout);
            printf("init_module %d failed\n", errno);
        }
        else {
            freopen("/early_services/dev/kmsg", "w", stdout);
            printf("init_module success for %s \n", audio_modules[i]);
        }
        close(fd);
        if(i == 3){
            fd = open("/sys/kernel/boot_adsp/boot", O_WRONLY);
            freopen("/early_services/dev/kmsg", "w", stdout);
                if (fd < 0) {
                    printf("open sys entry failed\n");
                } else if(-1 == write(fd, "1", 1)) {
                    printf("Write to sys entry failed\n");
                } else {
                    printf("ADSP firmware loading triggered\n");
                }
        close(fd);
        }
    }
    memset(marker, 0, 50);
    snprintf(marker, 49 ,"M - Insert Audio modules - End");
    write_marker(marker);
}

EnforcingStatus StatusFromCmdline() {
    EnforcingStatus status = SELINUX_ENFORCING;
    android::earlyinit::import_kernel_cmdline(false,
                          [&](const std::string& key, const std::string& value, bool in_qemu) {
            printf("ES: StatusFromCmdline inside import_kernel_cmdline\n");
                              if (key == "androidboot.selinux" && value == "permissive") {
                printf("ES: StatusFromCmdline permissive if\n");
                                  status = SELINUX_PERMISSIVE;
                              }
                          });
    printf("ES: StatusFromCmdline return status = %d\n", status);
    return status;
}

bool IsEnforcing() {
    return StatusFromCmdline() == SELINUX_ENFORCING;
}

BootStatus BootStatusFromCmdline() {
    BootStatus status = RECOVERY_BOOT;
    android::earlyinit::import_kernel_cmdline(false,
                          [&](const std::string& key, const std::string& value, bool in_qemu) {
            printf("ES: BootStatusFromCmdline inside import_kernel_cmdline\n");
                              if (key == "androidboot.force_normal_boot" && value == "1") {
                printf("ES: BootStatusFromCmdline NORMAL if\n");
                                  status = NORMAL_BOOT;
                              }
                          });
    printf("ES: BootStatusFromCmdline return status = %d\n", status);
    return status;
}

bool IsNormalBoot() {
    return BootStatusFromCmdline() == NORMAL_BOOT;
}


#define SECOND_STAGE "1"
#define FIRST_STAGE  "0"

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
        fd = open("/early_services/dev/socket/camera/soc_id", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
      else
        fd = open("/early_services/dev/socket/camera/platform_subtype_id", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

      write(fd,strName, strlen(strName) -1 );
      close(fd);
  }

  return 0;
}

int early_init(const char* stage)
{
  FILE* f;
  char line[LINE_MAX];
  int fd,pid,fd1;
  clearenv();
  setenv("PATH", DEFAULT_PATH, 1);
#ifdef TEMP_SOLUTION
  int ret;
  struct stat st = {0};

  clearenv();
  setenv("PATH", DEFAULT_PATH, 1);

  /* Mount early_services partition */
  /* TODO: Do not hard code dev node, sde54 is early_services_a partition */
  ret = mount("/dev/sde54", "/early_services", "ext4", MS_RDONLY, NULL);
  if (ret < 0) {
    perror("Mount early_serviecs partition failed");
    if (stat("/early_services", &st) == -1) {
      printf("/early_services directory doesn't exist\r\n");
    }
    if (stat("/dev/sde53", &st) == -1) {
      printf("/dev/sde53 doesn't exist \r\n");
    }
    /* Do not continue further */
    exit(-1);
  } else {
    printf("early_services partition mounted\r\n");
  }
  /* Chroot to early_services */
  ret = chroot("/early_services");
  if (ret < 0) {
    perror("chroot to /early_services failed");
  } else {
    printf("chroot to /early_services successful\n");
  }
  prepare_dir("dev");
#endif
  if (strcmp(stage, FIRST_STAGE) == 0) {
      prepare_dir("sysfs");
      prepare_dir("debugfs");
      prepare_dir("shm");
      prepare_dir("procfs");
      write_marker("M - EarlyInit FirstStage Start");
      mount("sysfs", "/sys", "sysfs", 0, NULL);
      mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL);
      mount("devtmpfs", "/early_services/dev", "devtmpfs", MS_NOSUID, "mode=0755");
      mknod("/dev/kmsg", S_IFCHR | 0600, makedev(1, 11));
      android::earlyinit::InitKernelLogging(NULL);
      LOG(INFO) << "ES : Logging enabled at early-services!";
      std::string precompiled_sepolicy_file = "/early_services/vendor/etc/selinux/precompiled_early_sepolicy";
      write_marker("M - EarlyInit SEPolicyLoad Start");
      //android::base::unique_fd fd1(open(precompiled_sepolicy_file.c_str(), O_RDONLY | O_CLOEXEC | O_BINARY));
      fd1 = open(precompiled_sepolicy_file.c_str(), O_RDONLY | O_CLOEXEC | O_BINARY, 0777);
      LOG(INFO) << "ES: precompiled sepolicy open fd=" << fd1;
      if (fd1 > 0) {
          if (selinux_android_load_policy_from_fd(fd1, precompiled_sepolicy_file.c_str()) < 0)
              LOG(INFO) << "Failed to load SELinux policy !";
          else
              LOG(INFO) << "ES : Successfully loaded precompiled sepolicy file";
      }
      close(fd1);
      write_marker("M - EarlyInit SEPolicyLoad End");
      bool is_enforcing = IsEnforcing();
      printf("ES : is_enforcing = %d\n", is_enforcing);
      if (security_setenforce(is_enforcing))
          LOG(INFO) << "Es: security_setenforce failed!";
      selinux_android_restorecon("/early_services/init_early",0);
      selabel_handle* sehandle = nullptr;
      sehandle = selinux_android_file_context_handle();
      selinux_android_set_sehandle(sehandle);
      setexeccon("u:r:init:s0");
      char *path = "/early_services/init_early";
      char *args[] = { path, SECOND_STAGE, NULL };
      execv(path,args);
  }
#ifdef LV_CODE
      /* Trigger firmware loading parallelly */
      trigger_firmware_loading(DRM_CARD_PATH);
#ifdef EARLY_ETHERNET
      if (-1 == mount("/dev/mmcblk0p42", "/persist","ext4", 0, NULL))
          perror("mount persist(mmcblk0p42) failed");
      trigger_firmware_loading(VIDEO_CARD_PATH);
#endif
#endif
  write_marker("M - Second Stage Start");
  set_permissions("/dev/kmsg", 0620, AID_ROOT, AID_SYSTEM, "u:object_r:kmsg_device:s0");
  android::earlyinit::InitKernelLogging(NULL);
  LOG(INFO) << "ES : In Second Stage!";

  bool is_normalBoot = IsNormalBoot();
  LOG(INFO) << "ES: Is Normal Boot " << is_normalBoot;
  /* Create ais_server socket dir and camera data dir */
  mkdir("/early_services/dev/socket", 0775);
  mkdir("/early_services/dev/socket/camera", 0775);

  getSysInfo("/sys/devices/soc0/soc_id",chipId);
  getSysInfo("/sys/devices/soc0/platform_subtype_id",platformId);

  set_permissions("/early_services/dev/dri/card3", 0666, AID_ROOT, AID_GRAPHICS, "u:object_r:graphics_device:s0");
  set_permissions("/early_services/dev/dri/card2", 0666, AID_ROOT, AID_GRAPHICS, "u:object_r:graphics_device:s0");
  set_permissions("/early_services/dev/null", 0666, AID_ROOT, AID_ROOT, "u:object_r:null_device:s0");
  set_permissions("/early_services/dev/urandom", 0666, AID_ROOT, AID_ROOT, "u:object_r:random_device:s0");
  set_permissions("/early_services/dev/media0", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/media1", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/video0", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/video1", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev1", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev2", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev3", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev4", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev5", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev6", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev7", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev8", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev9", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev10", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/socket/camera", 0775, AID_ROOT, AID_CAMERA, "u:object_r:vendor_camera_socket:s0");
  selinux_android_restorecon("/early_services/dev/socket/camera", SELINUX_ANDROID_RESTORECON_RECURSE);
  set_permissions("/early_services/dev/ion", 0664, AID_ROOT, AID_SYSTEM, "u:object_r:ion_device:s0");
  set_permissions("/early_services/dev/kgsl-3d0", 0664, AID_ROOT, AID_SYSTEM, "u:object_r:gpu_device:s0");
  set_permissions("/early_services/dev/v4l-subdev11", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev12", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev13", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev14", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev15", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev16", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/v4l-subdev0", 0660, AID_ROOT, AID_CAMERA, "u:object_r:video_device:s0");
  set_permissions("/early_services/dev/spidev1.0", 0666, AID_ROOT, AID_SYSTEM, "u:object_r:kmsg_device:s0");
  set_permissions("/dev/spidev1.0", 0666, AID_ROOT, AID_SYSTEM, "u:object_r:kmsg_device:s0");
  set_permissions("/early_services/dev/spidev22.0", 0666, AID_ROOT, AID_SYSTEM, "u:object_r:kmsg_device:s0");
  set_permissions("/dev/spidev22.0", 0666, AID_ROOT, AID_SYSTEM, "u:object_r:kmsg_device:s0");
  set_permissions("/early_services/dev/spidev10.0", 0666, AID_ROOT, AID_SYSTEM, "u:object_r:kmsg_device:s0");
  set_permissions("/dev/spidev10.0", 0666, AID_ROOT, AID_SYSTEM, "u:object_r:kmsg_device:s0");
  set_permissions("/dev/snd", 0777, AID_ROOT, AID_AUDIO, "u:object_r:audio_device:s0");
  set_permissions("/dev/snd/controlC0", 0666, AID_ROOT, AID_AUDIO, "u:object_r:audio_device:s0");

  /* Audio module loading parallely */

  pid = fork();
  if (pid < 0) {
    perror("fork insert audio modules failed \r\n");
  }

  if (0 == pid) {
    insert_audio_modules();
    exit(0);
  }

  f = fopen("/early_services/etc/early_init.conf", "re");
  if (f == NULL) {
      perror("open early_init.conf failed.\r\n");
      return -1;
  }
  selabel_handle* sehandle = nullptr;
  sehandle = selinux_android_file_context_handle();
  selinux_android_set_sehandle(sehandle);
  while (1) {
       if (!fgets(line, sizeof(line), f)) {
           if (feof(f))
               goto out;
           else {
               perror("read conf file meet error");
               goto out;
           }
       }
       if (is_empty_line(line))
           continue;

       if(is_normalBoot == 0)
           goto out;

       strstrip(line);
       parse_line(line);
       memset(line, 0, sizeof(line));
  }
out:
  fclose(f);
  write_marker("M - early-init-exit");
  mknod("/dev/sedone", S_IFREG | 0400, makedev(0,0));
  return 0;
}

int main(int argc, char* argv[])
{
    if (argc ==  1) {
        early_init(FIRST_STAGE);
    } else {
        early_init(SECOND_STAGE);
    }
    return 0;
}
