/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <linux/if.h>
#include <linux/sockios.h>
#include <linux/if_bridge.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include <sched.h>

#define LXC_ROOTFS_PATH         "/vendor_early_services/vendor/vm-system"
#define KPI_VALUE_PATH          "/sys/kernel/boot_kpi/kpi_values"

static void inline write_marker(const char* name)
{
    int fd = -1;

    fd = open(KPI_VALUE_PATH, O_WRONLY);
    if (fd > 0) {
        (void)write(fd, name, strlen(name));
    }
    close(fd);
    return;
}

static inline void print_log(const char* str)
{
    if (str == NULL) return;
    freopen("/dev/kmsg", "w", stdout);
    printf("ES: %s \r\n", str);
}

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

static inline int create_bridge(const char *br_name) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strlcpy(ifr.ifr_name, br_name, IFNAMSIZ);

    if (ioctl(sock, SIOCBRADDBR, &ifr) < 0) {
        perror("ioctl(SIOCBRADDBR)");
        close(sock);
        return -1;
    }

    close(sock);
    return 0;
}

extern int unshare(int __flags);
static void create_private_ns(void) {
	//Create new NS for current thread
	if (unshare(CLONE_NEWNS) != 0)
		print_log("Create new ns failed\n");

	//private rootfs to avoid mount escape
	mount("", "/", "", MS_REC | MS_PRIVATE, "");
}

static inline int start_lxc_container() {
    const char *dir = "/vendor_early_services/vendor/vm-system/lxc/bin";
    const char *lxc_path = "/vendor_early_services/vendor/vm-system/lxc/bin/lxc-start";
    const char *lxc_runtime_dir = "/vendor_early_services/run/lxc/run";

    int retries = 10;
    while (access(dir, X_OK) != 0 && retries-- > 0) {
        print_log("wait the lxc contatiner partion");
        usleep(100000); // 100ms
    }

    mkdirs(lxc_runtime_dir, 0777);
    pid_t pid = fork();
    //lxc-start -n lv -l debug --logfile=/vendor_early_services/run/lxc.log
    //lxc-start -n lv -l trace --logfile=/vendor_early_services/run/lxc.log
    if (pid == 0)
    {
        // char *const argv[] = { "lxc-start", "-n", "lv", NULL };
        char *const argv[] = {
            "lxc-start",
            "-n", "lv",
            "-l", "debug",
            "-o", "/vendor_early_services/run/lxc.log",
            "--logfile=/vendor_early_services/run/lxc.log",
            NULL
        };

        //Create new private ns before exec LXC
        create_private_ns();
        execv(lxc_path, argv);
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            print_log("execv lxc-start success!");
            return 0;
        } else {
            print_log("exec lxc-start failed");
            return -1;
        }
    } else {
        printf("fork failed %s\n", strerror(errno));
        return -1;
    }
}

int main(int argc, char *argv[]){
    int status = 0;

    print_log("start lxc");
    write_marker("M - ES lxc start -- begin");

    if (create_bridge("lxcbr0") == 0) {
        print_log("lxcbr0 created successfully!");
    } else {
        print_log("lxcbr0 created failed!");
    }

    status = start_lxc_container();
    if (status != 0) {
        print_log("Failed to start LXC container!");
    } else {
        print_log("Success to start LXC container!");
        write_marker("M - ES lxc start -- done");
    }

    return 0;
}
