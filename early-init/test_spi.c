/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <sys/types.h>
#include <ctype.h>
#include <getopt.h>
#include <time.h>
#include <linux/spi/spidev.h>

#include <linux/types.h>
#include <linux/ioctl.h>

#define SPIDEVTEST_BUFLEN   16   /* one block */
#define SPIDEVTEST_DEVLEN   50
#define ARRAY_SIZE(a)       (int)(sizeof(a) / sizeof((a)[0]))
#define ALIGN(x, a)     (((x) + (a) - 1) & ~((a) - 1))

static uint8_t saved_mode;
static uint32_t max_speed;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
#define KPI_VALUE_PATH          "/sys/kernel/boot_kpi/kpi_values"
#endif

static void inline write_marker(const char* name)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
    int fd = -1;

    fd = open(KPI_VALUE_PATH, O_WRONLY);
    if (fd > 0) {
        (void)write(fd, name, strlen(name));
    }
    close(fd);
#else
    int fd = freopen("/early_services/dev/kmsg", "w", stdout);
    if (fd > 0) {
        printf("boot_kpi: %s", name);
        close(fd);
    }
#endif
    return;
}

static int dev_open(const char *dev_node)
{
    int fd = -1;
    char buf[SPIDEVTEST_DEVLEN];

    snprintf(buf, sizeof(buf), "/dev/%s", dev_node);
    while (fd <= 0){
    fd = open(buf, O_RDWR);
    usleep(10000);
    }
    return fd ;
}

static int set_mode(int fd, uint8_t spi_mode)
{
    return ioctl(fd, SPI_IOC_WR_MODE, &spi_mode);
}

static int save_mode(int fd)
{
    return ioctl(fd, SPI_IOC_RD_MODE, &saved_mode);
}


static void fill_random_buffer(uint8_t *buf, int len)
{
    int i;
    srand(time(NULL));
    for (i = 0; i < len; i++)
        buf[i] = (uint8_t)(rand() & 0xFF);
}

static int send_buffer(int fd, struct spi_ioc_transfer *data,
               uint8_t spi_mode)
{
    int rc;

    rc = set_mode(fd, spi_mode);
    if (rc)
        goto do_exit;
    if (max_speed)
        data->speed_hz = max_speed;

    /* Do full-duplex transfer */
    rc = ioctl(fd, SPI_IOC_MESSAGE(1), data);
    if (rc <= 0) {
        rc = -1;
        goto do_exit;
    }

    if (rc != (int)data->len) {
        goto do_exit;
    }

    return 0;

do_exit:
    return rc;
}

static int send_loopback_buffer(int fd, struct spi_ioc_transfer *data)
{
    int rc,req_mode = SPI_MODE_0;

    rc = send_buffer(fd, data, req_mode | SPI_LOOP);
    if (rc)
        goto do_exit;

    /* Check rx buffer is equal to tx buffer */
    rc = memcmp((void *)(uintptr_t)data->rx_buf,
            (void *)(uintptr_t)data->tx_buf, data->len);
do_exit:
    return rc;
}

/* Send all possible sizes and modes */
static int send_all_loopback_buffer(int fd, struct spi_ioc_transfer *data)
{
    int rc, test_result = 0;

            data->bits_per_word = 8;
            data->len = ALIGN(data->len, data->bits_per_word/8);
            rc = send_loopback_buffer(fd, data);
            test_result |= rc;
    return test_result;
}

/* Configure device to use loopback mode and transfer data */
static int test_loopback(int fd)
{
    uint8_t tx_buf[SPIDEVTEST_BUFLEN] = {0};
    uint8_t rx_buf[ARRAY_SIZE(tx_buf)] = {0};
    struct spi_ioc_transfer data = {
        .tx_buf = (unsigned long)tx_buf,
        .rx_buf = (unsigned long)rx_buf,
        .len = ARRAY_SIZE(tx_buf),
    };

    fill_random_buffer(tx_buf, ARRAY_SIZE(tx_buf));
    return send_loopback_buffer(fd, &data);
}

int nominal_test(int fd)
{
    int rc;

    write_marker("M - ES SPI loopback test begin");
    rc = test_loopback(fd);
    if(rc == 0)
        write_marker("M - ES SPI loopback test finish");
    else
        write_marker("M - ES SPI loopback test failed");
    return rc;
}

int main(int argc, char **argv)
{
    int fd;
    if(argc != 2) {
        printf("Usage: %s <spidev node name>\ni.e.: \n\t %s spidev1.0\n",argv[0],argv[0]);
        return -1;
    }

    fd = dev_open(argv[1]);
    if (fd <= 0)
        return 1;
    nominal_test(fd);
    return 0;
}
