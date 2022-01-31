/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
/*
int echoToFile(char *str, int cnt, char *path) {
	int fd, ret1;

	fd = open(path,O_RDWR| O_APPEND);
	if( fd >= 0 ) {
		ret1 = write(fd, str, cnt);
		if( ret1 >= 0 ) {
			freopen("/dev/kmsg", "w", stdout);
			printf("Write successful %d  ->  %d\r\n",fd, ret1);
		} else {
			freopen("/dev/kmsg", "w", stdout);
			printf("Write failed %d  ->  %d  %d\r\n",fd, ret1, errno);
		}
		close(fd);
	} else {
		creat(path, S_IRWXU);
		freopen("/dev/kmsg", "w", stdout);
		printf("File Created %d  \r\n",fd);
	}
	return fd;
}
*/
int main(int argc, char *argv[]){

        freopen("/dev/kmsg", "w", stdout);
        printf("Hello World \r\n");
#if 0
	void *mylib;
	int eret;
	struct stat st = {0};
	int acc_ret, acc_errno, count = 0;
	char path[] = "/early_services/dev/socket/camera/test";
	char msg[] = "This is test -> ";
	char testmsg[256];

        while(1) {
		mylib = dlopen("/early_services/system/lib64/libc2d30_bltlib.so", RTLD_LOCAL | RTLD_LAZY);
		if (!mylib) {
		        /* fail to load the library */
                        freopen("/dev/kmsg", "w", stdout);
			printf("==== Swap_dbg dlopen Error: %s\n", dlerror());
		} else {
                        freopen("/dev/kmsg", "w", stdout);
			printf("==== Swap_dbg dlopen Success: %s\n", dlerror());
			dlclose(mylib);
		}
		if (stat("/early_services/dev/dri/card3", &st) == -1) {
                        freopen("/dev/kmsg", "w", stdout);
			printf("==== Swap_dbg stat /early_services/dev/dri/card3 fail\n");
                } else {
                        freopen("/dev/kmsg", "w", stdout);
			printf("==== Swap_dbg stat /early_services/dev/dri/card3 success\n");
		}
		acc_ret = access("/early_services/dev/random", F_OK);
		acc_errno = errno;
		if(acc_ret) {
			freopen("/dev/kmsg", "w", stdout);
			printf(" Dev directory not accessable %d %d\r\n",acc_ret, acc_errno);
		} else {
			freopen("/dev/kmsg", "w", stdout);
			printf("Dev Directory accessible %d %d\r\n",acc_ret, acc_errno);
		}
		sleep(1);
	}
#endif
	return 0;
}
