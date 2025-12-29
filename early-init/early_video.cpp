/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>

#define EARLY_VIDEO_DEC_DEVICE            "/dev/video32"
#define MAX_KPI_VALUE_LENGTH 256

static void place_marker(char const *fmt, ...)
{
  int fd = open("/sys/kernel/boot_kpi/kpi_values", O_WRONLY);
  if (fd > 0) {
    static char buf[MAX_KPI_VALUE_LENGTH] = {0};
    va_list args;
    int len;
    va_start(args, fmt);
    len = vsnprintf(buf, MAX_KPI_VALUE_LENGTH, fmt, args);
    va_end(args);
    ssize_t ret = write(fd, buf, len);
    if (ret < 0) {
      printf("write bootkpi failed %s\r\n", strerror(errno));
    }
    close(fd);
  } else {
    printf("open bootkpi for %s failed %s\r\n", fmt, strerror(errno));
  }
 }

int main()
{
  usleep(200000);
  freopen("/dev/kmsg", "w", stdout);
  place_marker("earlyVideo - Start!!!");
  int driver_fd = open(EARLY_VIDEO_DEC_DEVICE, O_RDWR);
  if (driver_fd < 0) {
    printf("earlyVideo open fd failed %d, errno %d\n", driver_fd, errno);
    place_marker("earlyVideo open fd failed errno %d - Exit", errno);
    return -1;
  }
  place_marker("earlyVideo - open fd success");
  close(driver_fd);
  place_marker("earlyVideo - Exit");

  return 0;
}
