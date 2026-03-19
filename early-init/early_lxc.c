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

static inline void print_log(const char* fmt, ...)
{
    if (fmt == NULL) return;
    freopen("/dev/kmsg", "w", stdout);

    va_list args;
    va_start(args, fmt);

    printf("ES: ");
    vprintf(fmt, args);
    printf("\r\n");

    va_end(args);
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

static void wait_for_mount_point() {
	const char *check_paths[] = {
        "/dev/dma_heap",
        "/dev/dri/renderD128",
        "/dev/dri/card2",
        "/dev/kgsl-3d0",
        "/dev/snd",
        "/dev/socket/agm",
    };
	int i = 0, retry = 0;
	int num_paths = sizeof(check_paths) / sizeof(check_paths[0]);
	for (i = 0; i < num_paths ; i++) {
		retry = 0;
		while (retry++ < 1500) {
			if (access(check_paths[i], F_OK) == 0) {
				print_log("check path okay %s \n", check_paths[i]);
				break;
			} else {
				if (retry % 20 == 1)
					print_log("check path failed %s \n", check_paths[i]);
				usleep(100 * 1000);//sleep 100ms
			}
		}
	}
	//flush log buffer, will removed once all MM ready
	print_log("\n");
	usleep(100 * 1000);
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
    const char *monitor_path = "/vendor_early_services/vendor/vm-system/lxc/bin/lxc-monitor";


    int retries = 10;
    while (access(dir, X_OK) != 0 && retries-- > 0) {
        print_log("wait the lxc contatiner partion");
        usleep(100000); // 100ms
    }

    wait_for_mount_point();
    pid_t pid_start = fork();
    //lxc-monitor -n lv -W -o /vendor_early_services/run/lxc_monitor.log
    //lxc-start -n lv -l trace --logfile=/vendor_early_services/run/lxc.log
    if (pid_start == 0)
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
        pid_t pid_monitor = fork();
        if (pid_monitor == 0) {
            char *const argv_m[] = {
                "lxc-monitor",
                "-n", "lv",
                "-W",
                "-l", "debug",
                "-o", "/vendor_early_services/run/lxc_monitor.log",
                NULL
            };
            execv(monitor_path, argv_m);
        } else if (pid_monitor > 0) {
            execv(lxc_path, argv);
            _exit(127);
        } else
            _exit(127);
    } else if (pid_start > 0) {
        int status;
        waitpid(pid_start, &status, 0);
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
    return 0;
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
        write_marker("M - ES lxc start failed");
    } else {
        print_log("Success to start LXC container!");
        write_marker("M - ES lxc start -- done");
    }

    return 0;
}
