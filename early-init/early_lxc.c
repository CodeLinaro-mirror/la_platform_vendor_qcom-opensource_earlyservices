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

#define LXC_ROOTFS_PATH         "/vendor_early_services/vendor/vm-system"
#define LXC_ROOTFS_BLOCK_DEVICE "/dev/block/sde22"
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

static inline int mount_lxc_rootfs() {
    const char *mount_label = "/dev/block/sde22";
    int i = 0;

    if (access(LXC_ROOTFS_PATH, F_OK) == -1) {
        print_log(" LXC_ROOTFS_PATH doesn't exist");
        mkdir(LXC_ROOTFS_PATH, 0777);
    }

    if (access(LXC_ROOTFS_BLOCK_DEVICE, F_OK) == -1) {
        print_log(" /dev/block/sde22 doesn't exist");
    }

    if (access(LXC_ROOTFS_PATH, F_OK) == 0) {
        if (mount(LXC_ROOTFS_BLOCK_DEVICE, LXC_ROOTFS_PATH, "ext4", MS_RDONLY, "context=u:object_r:same_process_hal_file:s0") < 0) {
            fprintf(stderr, "lxc_rootfs mount failed: %s\n", strerror(errno));
            return -1;
        } else {
            print_log(" lxc_rootfs mount successfully");
            return 0;
        }
    } else {
        print_log(" lxc_rootfs directory is not created!");
        return -1;
    }
}

static inline int start_lxc_container() {
    const char *dir = "/vendor_early_services/vendor/vm-system/lxc/bin";
    const char *lxc_path = "/vendor_early_services/vendor/vm-system/lxc/bin/lxc-start";

    int retries = 10;
    while (access(dir, X_OK) != 0 && retries-- > 0) {
        print_log("wait the lxc contatiner partion");
        usleep(100000); // 100ms
    }

    if (access(lxc_path, X_OK) != 0) {
        print_log("No lxc-start binary file!");
        return -1;
    } else {
        print_log("the /vendor_early_services/vendor/vm-system/lxc/bin/lxc-start is ok");
    }

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

    if (mount_lxc_rootfs() == 0) {
        status = start_lxc_container();
        if (status != 0) {
            print_log("Failed to start LXC container!");
        } else {
            print_log("Success to start LXC container!");
            write_marker("M - ES lxc start -- done");
        }
    } else {
        print_log("lxc rootfs not mount, can't start lxc!");
    }

    return 0;
}
