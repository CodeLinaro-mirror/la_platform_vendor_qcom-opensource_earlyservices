/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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
* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#define _GNU_SOURCE
#define TEMP_SOLUTION
#define DEBUG

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
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

#define DEFAULT_CONF            "/etc/early_init.conf"
#define END_TAG                 "<end>"
#define LINE_MAX                2048
#define WHITESPACE              " \t\n\r"
#define KPI_VALUE_PATH          "/sys/kernel/debug/bootkpi/kpi_values"
#define GPIO_EXPORT             "/sys/class/gpio/export"
#define DRM_CARD_PATH           "/dev/dri/card0"
#define VIDEO_CARD_PATH         "/dev/video32"
#define AUDIO_FW_PATH           "/vendor/firmware_mnt"
//#define DISPLAY_XDG_RUNTIME_DIR "/run/platform/weston"
#define SMACK_LABEL_PATH        "/proc/self/attr/current"
#define SMACK_LABEL             "System"
#define	DEFAULT_PATH		"/sbin:/system/sbin:/system/bin:/system/xbin:/odm/bin:/vendor/bin:/vendor/xbin"

#define STR_EXPAND(tok) #tok
#define TO_STRING(tok) STR_EXPAND(tok)

static struct appinfo {
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

static int list;
static struct appinfo **wlist_app;

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
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	int fd = -1;

	fd = open(KPI_VALUE_PATH, O_WRONLY);
	if (fd > 0) {
		(void)write(fd, name, strlen(name));
	} else {
		printf("open bootkpi for name %s failed %s\r\n", name, strerror(errno));
	}
	safe_close(fd);
#else
	int fd = freopen("/dev/kmsg", "w", stdout);
        if (fd > 0) {
	     printf("boot_kpi: %s\n", name);
             close(fd);
        }
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
static inline void mkdirs(char* p, mode_t mode)
{
	char str[1024] = {0};
	struct stat st = {0};
	int i = 0, len = 0, ret = 0;

	len = strlen(p);
	if (len > 1024)
		printf("input string is too long\r\n");

	strlcpy(str, p, sizeof(str));

	if (str[0] != '/')
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

	for (i = 0; i < app_launcher.env_used; i++)
		safe_free(&app_launcher.env[i]);

	app_launcher.argv_used = 0;
	app_launcher.env_used = 0;
	app_launcher.bindcpumask = -1;
	app_launcher.priority = -1;
	app_launcher.env[app_launcher.env_used++] = DEFAULT_PATH; //set DEFAULT_PATH as static env[0] path for all ES app's

	return;
}

static void inline app_launcher_dup(struct appinfo *dup)
{
	int i = 0;

	if (app_launcher.appname) {
		dup->appname = strdup(app_launcher.appname);
	}
	if (app_launcher.cmd) {
		dup->cmd = strdup(app_launcher.cmd);
	}
	if (app_launcher.applog) {
		dup->applog = strdup(app_launcher.applog);
	}
	if (app_launcher.gpio) {
		dup->gpio = strdup(app_launcher.gpio);
	}
	if (app_launcher.pidfile) {
		dup->pidfile = strdup(app_launcher.pidfile);
	}
	if (app_launcher.wait) {
		dup->wait = strdup(app_launcher.wait);
	}
	if (app_launcher.username) {
		dup->username = strdup(app_launcher.username);
	}
	if (app_launcher.usleep > 0) {
		dup->usleep = app_launcher.usleep;
	}

	if (app_launcher.argv_used > 0) {
		for (i = 0; i < app_launcher.argv_used; i++) {
			dup->argv[i] = strdup(app_launcher.argv[i]);
		}
	}

	if (app_launcher.env_used > 0) {
		for (i = 0; i < app_launcher.env_used; i++) {
			dup->env[i] = strdup(app_launcher.env[i]);
		}
	}

	if (app_launcher.argv_used > 0) {
		dup->argv_used = app_launcher.argv_used;
	}
	if (app_launcher.env_used > 0) {
		dup->env_used = app_launcher.env_used;
	}
	if (app_launcher.bindcpumask != 0) {
		dup->bindcpumask = app_launcher.bindcpumask;
	}
	if (app_launcher.priority > 0) {
		dup->priority = app_launcher.priority;
	}
	dup->env[dup->env_used++] = DEFAULT_PATH; //set DEFAULT_PATH as static env[0] path for all ES app's

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
				printf("appname is %s \r\n", app_launcher.appname);
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
				printf("applog is %s \r\n", app_launcher.applog);
			}
			break;
		case 'g':/* gpio */
			if (0 == strncmp(p + 1, "pio", strlen("pio")) && 0 == find_rvalue(&p)) {
				app_launcher.gpio = strdup(p);
				printf("gpio is %s \r\n", app_launcher.gpio);
			}
			break;
		case 'w':/* wait */
			if (0 == strncmp(p + 1, "ait", strlen("ait")) && 0 == find_rvalue(&p)) {
				app_launcher.wait = strdup(p);
				printf("wait is %s \r\n", app_launcher.wait);
			}
			break;
		case 'p':/* pidfile */
			if (0 == strncmp(p + 1, "idfile", strlen("idfile")) && 0 == find_rvalue(&p)) {
				app_launcher.pidfile = strdup(p);
				printf("pidfile is %s \r\n", app_launcher.pidfile);
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
				printf("username is %s \r\n", app_launcher.username);
			}
			break;
		case 'r':
			if (0 == strncmp(p + 1, "estart", strlen("estart")) && 0 == find_rvalue(&p)) {
				if (0 == strncmp(p, "true", strlen("true"))) {
					wlist_app = (struct appinfo **)realloc(wlist_app, (list + 1) * sizeof(struct appinfo *));
					wlist_app[list] = (struct appinfo *)malloc(sizeof(struct appinfo));
					app_launcher_dup(wlist_app[list++]);
				}
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
					for (int i = 0; i < 4; i++) {
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
//					for (i = 0; i < 30; i++) {
					while(1) { /* TODO: find a finite value for wait */
						if (-1 != access(app_launcher.wait, F_OK))
							break;
						usleep(5000);
					}
				}

				if (app_launcher.usleep > 0)
					usleep(app_launcher.usleep);

				app_launcher.argv[app_launcher.argv_used] = NULL;
				app_launcher.env[app_launcher.env_used] = NULL;

			//	write_smack_label(SMACK_LABEL);

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
						snprintf(marker, 49 ,"M - Launch %s app failed", app_launcher.appname);
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
	const char modprobe_command[256] = "modprobe -a -d /vendor/lib/modules audio_adsp_loader audio_q6 audio_native audio_swr audio_platform audio_stub audio_machine_talos audio_apr audio_q6_notifier";
	struct stat st = {0};
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
		snprintf(marker, 49 ,"M - Insert Audio modules - Start");
		write_marker(marker);

		system(modprobe_command);

		memset(marker, 0, 50);
		snprintf(marker, 49 ,"M - Insert Audio modules - End");
		write_marker(marker);

		do{
			printf("Waiting for sys entry to set boot_adsp flag\n");
			usleep(2000);
			/* Do Nothing */
		}while(stat("/sys/kernel/boot_adsp/boot",&st) == -1);

		fd = open("/sys/kernel/boot_adsp/boot", O_WRONLY);
		if (fd < 0) {
			perror("open sys entry failed \r\n");
		} else if(-1 == write(fd, "1", 1)) {
			perror("Write to sys entry failed\n");
		} else {
			printf("ADSP firmware loading triggered\n");
			memset(marker, 0, 50);
			snprintf(marker, 49 ,"M - ADSP firmware loading triggered");
			write_marker(marker);
		}
		exit(0);
	}

	return;
}

static void sigchild_handler(int sig, siginfo_t *siginfo, void *context)
{
	int i = 0, fd = 0, pid = 0, ret = 0;
	char pid_str[10] = {0};
	static char marker[50] = {0};
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	char timestr[10] = {0};

	if (list == 0) {
		return;
	}

	for(i = 0; i < list; i++) {
		if (!wlist_app[i]->pidfile) {
			continue;
		}

		fd = open(wlist_app[i]->pidfile, O_RDONLY);
		if(fd < 0) {
			perror("open");
		}
		ret = read(fd, pid_str, 10);
		if(ret < 0) {
			perror("read");
		}
		safe_close(fd);
		pid = atoi(pid_str);

		if(siginfo->si_pid == pid) {
			printf("Child of pid %d terminated; Restarting it...\n", siginfo->si_pid);

			pid = fork();

			if (pid < 0) {
				perror("fork child process failed \r\n");
			} else if (0 == pid) {
				if (wlist_app[i]->applog) {
					strftime(timestr, sizeof(timestr)-1, "%H%M%S", t);
					strlcat(wlist_app[i]->applog, timestr, 6);
					fd = open(wlist_app[i]->applog, O_RDWR | O_CREAT, 0666);
					if (fd > 0) {
						dup2(fd, fileno(stdout));
						dup2(fd, fileno(stderr));
						safe_close(fd);
						safe_close(fd);
					}
				}

				if (wlist_app[i]->bindcpumask != -1) {
					cpu_set_t mask;
					CPU_ZERO(&mask);
					for (i = 0; i < 4; i++) {
						if (BIT_SET(wlist_app[i]->bindcpumask, i))
							CPU_SET(i, &mask);
					}
					if (0 != sched_setaffinity(0, sizeof(mask), &mask))
						printf("sched_setaffinity failed %d %s\r\n", wlist_app[i]->bindcpumask, strerror(errno));
				}

				if (wlist_app[i]->priority > 0) {
					struct sched_param sp;
					memset( &sp, 0, sizeof(sp) );
					sp.sched_priority = wlist_app[i]->priority;
					if (0 != sched_setscheduler( 0, SCHED_FIFO, &sp))
						printf("sched_setparam failed %d %s\r\n", wlist_app[i]->priority, strerror(errno));
				}

				if (wlist_app[i]->gpio) {
					fd = open(GPIO_EXPORT, O_WRONLY);
					if (fd < 0) {
						perror("open gpio export node failed \r\n");
					} else {
						if (-1 == write(fd, wlist_app[i]->gpio,strlen(wlist_app[i]->gpio))) {
							printf("config gpio to %s failed: %s\n", wlist_app[i]->gpio, strerror(errno));
						}
					}
					safe_close(fd);
				}

				memset(pid_str, 0, 10);
				if (wlist_app[i]->pidfile) {
					remove(wlist_app[i]->pidfile);
					snprintf(pid_str, 10, "%d" ,getpid());

					fd = open(wlist_app[i]->pidfile, O_CREAT | O_RDWR, 0666);
					if(fd < 0) {
						perror("open");
					}
					if (-1 == write(fd, pid_str, 10)) {
						printf("write pidfile %s failed: %s\n", wlist_app[i]->pidfile, strerror(errno));
					}
					safe_close(fd);
#ifdef DEBUG
					memset(pid_str, 0, 10);
					fd = open(wlist_app[i]->pidfile, O_RDONLY);
					if(fd < 0) {
						perror("open");
					}
					ret = read(fd, pid_str, 10);
					if(ret < 0) {
						perror("read");
					}
					printf("changed pid %s\n", pid_str);
					safe_close(fd);
#endif
				}

				/*
				 * Wait for early_driver
				 */
				if (wlist_app[i]->wait) {
					printf("app %s waiting for %s ...\r\n", wlist_app[i]->appname, wlist_app[i]->wait);
					//					for (i = 0; i < 30; i++) {
					while(1) { /* TODO: find a finite value for wait */
						if (-1 != access(wlist_app[i]->wait, F_OK)){
							break;
						}
						usleep(5000);
					}
				}

				wlist_app[i]->argv[wlist_app[i]->argv_used] = NULL;
				wlist_app[i]->env[wlist_app[i]->env_used] = NULL;

				//	write_smack_label(SMACK_LABEL);

				if (wlist_app[i]->username) {
					enforce_user(wlist_app[i]->username);
				}
				memset(marker, 0, 50);
				snprintf(marker, 49 ,"M - Relaunch %s app", wlist_app[i]->appname);
				write_marker(marker);

				if (wlist_app[i]->cmd) {
					ret = execvpe(wlist_app[i]->cmd, wlist_app[i]->argv, wlist_app[i]->env);
					if(ret < 0) {
						printf("App launch failed %s \r\n", wlist_app[i]->appname);
						memset(marker, 0, 50);
						snprintf(marker, 49 ,"M - Relaunch %s app failed", wlist_app[i]->appname);
						write_marker(marker);
					}
				}
				printf("Restarted %s \r\n", wlist_app[i]->appname);
				exit(0);
			}
			break;
		}
	}
}


int early_init(void)
{
	FILE* f;
	char line[LINE_MAX];
	int fd;
	struct sigaction sig;
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
	prepare_dir("sysfs");
	prepare_dir("debugfs");
	prepare_dir("xdg_runtime_dir");
	prepare_dir("shm");
	prepare_dir("procfs");
	//prepare_dir("audio_fw");

	fd = open("/run/early_init.log", O_RDWR | O_CREAT, 0666);
	if (fd < 0)
		perror("open log file failed");

	dup2(fd, fileno(stdout));
	dup2(fd, fileno(stderr));
	safe_close(fd);
	safe_close(fd);

	f = fopen(DEFAULT_CONF, "re");
	if (f == NULL) {
		perror("open early_init.conf failed.\r\n");
		return -1;
	}

	write_marker("M - early-init-start-up");
#ifdef LV_CODE
	/* Trigger firmware loading parallelly */
	trigger_firmware_loading(DRM_CARD_PATH);
#ifdef EARLY_ETHERNET
	if (-1 == mount("/dev/mmcblk0p42", "/persist","ext4", 0, NULL))
		perror("mount persist(mmcblk0p42) failed");
	trigger_firmware_loading(VIDEO_CARD_PATH);
#endif
#endif
	insert_audio_modules();
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

		strstrip(line);
		parse_line(line);
		memset(line, 0, sizeof(line));
		/* write_marker("early-init-line...."); */
	}
out:
	fclose(f);
	write_marker("M - early-init-exit");

	sig.sa_sigaction = &sigchild_handler;
	sig.sa_flags = SA_SIGINFO;
	if(sigaction(SIGCHLD, &sig, NULL) < 0) {
		perror("sigaction");
	}

	while (1) {
		ret = wait(NULL);
		if(ret < 0) {
			perror("wait");
			break;
		}
	}

	return 0;
}

int main(int argc, char* argv[])
{
	int pid = 0;
	char *android_init_argv[2];
#ifndef TEMP_SOLUTION
	int ret;
	struct stat st = {0};
#endif

#ifdef DEBUG
	printf("Welcome to Early Userspace solution\n");
#endif
	pid = fork();
	if (pid < 0) {
		printf("Fork failed\n");
		exit(-1);
	} else if (0 == pid) { //child process
		early_init();
	} else {
	#ifndef TEMP_SOLUTION
		/* Final solution */
		/* Mount system partition */
		/* TODO: Do not hard code dev node, sda6 is system  partition */
		ret = mount("/dev/sda6", "/system", "ext4", MS_RDONLY, NULL); //sda6 is system partition
		if (ret < 0) {
			perror("Mount system partition failed");
			if (stat("/system", &st) == -1) {
				printf("/system directory doesn't exist\r\n");
			}
			if (stat("/dev/sda6", &st) == -1) {
				printf("/dev/sda6 doesn't exist \r\n");
			}
		} else {
			printf("system partition mounted\r\n");
		}

		/* Chroot to system */
		ret = chroot("/system");
		if (ret < 0) {
			perror("chroot to /system failed");
		} else {
			printf("chroot to /system successful\r\n");
		}

		/* Exec Android init */
	#endif
		printf("Start Android init \r\n");

		android_init_argv[0] = "/init";
		android_init_argv[1] = NULL;
		execv("/init",android_init_argv);
		printf("exec failed\n");
	}
	return 0;
}
