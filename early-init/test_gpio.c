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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>
#include <linux/version.h>

#define MAX_BUFF_LEN 256
#define MSEC_SLEEP_DURATION 1000

int gpioread(int gpio_num) {
    volatile int fd = -1,ret = -1;
    int fd_kpi,i;
    char num_str[MAX_BUFF_LEN] = {0};
    char print_str[MAX_BUFF_LEN] = {0};

    while(fd < 0) {
        fd = open("/sys/class/gpio/export", O_WRONLY);
        usleep(MSEC_SLEEP_DURATION);
    }

    snprintf(num_str,sizeof(num_str),"%d",gpio_num);

    ret = write(fd,num_str,strlen(num_str));
    if(ret < 0 ){
        freopen("/early_services/dev/kmsg", "w", stdout);
        printf("Error writing gpio export \r\n");
        return -1;
    }
    close(fd);
    snprintf(num_str,sizeof(num_str),"/sys/class/gpio/gpio%d/direction",gpio_num);
    while(ret < 0) {
        ret= access(num_str,F_OK);
        usleep(MSEC_SLEEP_DURATION);
    }

    fd = open(num_str, O_WRONLY);
    if(fd < 0 ){
        freopen("/early_services/dev/kmsg", "w", stdout);
        printf("Error opening gpio direction \r\n");
        return -1;
    }
    ret = write(fd,"in",2);
    if(ret < 0 ){
        freopen("/early_services/dev/kmsg", "w", stdout);
        printf("Error writing gpio direction \r\n");
        return -1;
    }
    close(fd);

    snprintf(num_str,sizeof(num_str),"/sys/class/gpio/gpio%d/value",gpio_num);

    while(ret < 0) {
        ret= access(num_str,F_OK);
        usleep(MSEC_SLEEP_DURATION);
    }

    fd = open(num_str, O_RDONLY);
    if(fd < 0 ){
        freopen("/early_services/dev/kmsg", "w", stdout);
        printf("Error opening gpio value \r\n");
        return -1;
    }
    memset(num_str,0,sizeof(num_str));
    ret = read(fd,num_str,2);
    if(ret < 0 ){
        freopen("/early_services/dev/kmsg", "w", stdout);
        printf("Error reading gpio value \r\n");
        return -1;
    }
    close(fd);

    freopen("/early_services/dev/kmsg", "w", stdout);
    printf(" gpio %d value is = %s\r\n",gpio_num,num_str);
    snprintf(print_str,sizeof(print_str),"M - ES GPIO %d VALUE IS = %c",gpio_num,num_str[0]);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
    fd_kpi = open("/sys/kernel/boot_kpi/kpi_values", O_RDWR);
    if(fd_kpi > 0) {
        write(fd_kpi , print_str,strlen(print_str));
        close(fd_kpi);
    }
#else
    if (freopen("/early_services/dev/kmsg", "w", stdout)) {
        printf("boot_kpi: %s", print_str);
    }
#endif
    return ret;
}

int main(int argc, char *argv[]){

    if ( argc != 2 ) {
        printf("Usage:\n %s <gpio_number>\n",argv[0]);
        return -1;
    }

    gpioread(atoi(argv[1]));
    return 0;
}
