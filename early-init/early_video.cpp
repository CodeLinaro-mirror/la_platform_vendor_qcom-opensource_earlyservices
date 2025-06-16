/*******************************************************************************
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries. 
SPDX-License-Identifier: BSD-3-Clause-Clear

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#define EARLY_VIDEO_DEC_DEVICE            "/dev/video32"


int main()
{
  freopen("/dev/kmsg", "w", stdout);
  printf("start earlyVideo\n");
  int driver_fd = open(EARLY_VIDEO_DEC_DEVICE, O_RDWR);
  if (driver_fd < 0) {
    printf("earlyVideo open fd failed %d, errno %d\n", driver_fd, errno);
    return -1;
  }
  printf("earlyVideo open fd success\n");
  close(driver_fd);

  return 0;
}
