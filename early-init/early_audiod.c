/*******************************************************************************
Copyright (c) 2021 The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>

#include <tinyalsa/asoundlib.h>
#include "early_audiod.h"

static struct snd_card_info *info = NULL;
int pcm_id[4];

#ifdef PLATFORM_MSMNILE

#define PCM_ID_1_TX     TERT_TDM_TX_HOSTLESS
#define PCM_ID_1_RX     SEC_TDM_RX_HOSTLESS
#define PCM_ID_2_TX     QUIN_TDM_TX_HOSTLESS
#define PCM_ID_2_RX     QUAT_TDM_RX_HOSTLESS

#elif PLATFORM_MSMSTEPPE

#define PCM_ID_1_TX     TERT_TDM_TX_HOSTLESS
#define PCM_ID_1_RX     QUIN_TDM_RX_HOSTLESS
#define PCM_ID_2_TX     QUAT_TDM_TX_HOSTLESS
#define PCM_ID_2_RX     QUAT_TDM_RX_HOSTLESS

#endif

const char *audio_route[MAX_SESSION] = {
#ifdef PLATFORM_MSMNILE
    {"SEC_TDM_RX_7 Port Mixer TERT_TDM_TX_7"},
    {"QUAT_TDM_RX_7 Port Mixer QUIN_TDM_TX_7"}
#elif PLATFORM_MSMSTEPPE
    {"QUIN_TDM_RX_7 Port Mixer TERT_TDM_TX_7"},
    {"QUAT_TDM_RX_7 Port Mixer QUAT_TDM_TX_7"}
#else
    {""},
    {""}
#endif
};
// The mutex for place marker.
static pthread_mutex_t marker_mutex;

void set_snd_card_enable_hostless(int snd_card)
{
    auto_audio_ext_set_snd_card(snd_card);
    auto_audio_ext_enable_hostless();
}

static int32_t auto_audio_ext_set_mixer_ctl(struct mixer *mixer, const char *name , int value)
{
    int32_t ret = 0, i = 0;
    struct mixer_ctl *ctl = NULL;
    char buf[32];

    if (!mixer || !name) {
        freopen("/dev/kmsg", "w", stdout);
        printf("mixer ctl is NULL");
        return -1;
    }

    ctl = mixer_get_ctl_by_name(mixer, name);
    if (!ctl) {
        freopen("/dev/kmsg", "w", stdout);
	printf("Could not get ctl for mixer by name");
        return -1;
    }

    ret = mixer_ctl_set_value(ctl, 0, value);
    if (ret) {
        freopen("/dev/kmsg", "w", stdout);
	printf("Could not set ctl for mixer");
    }

    return ret;
}

/* Note: Due to ADP H/W design, SoC TERT/SEC TDM CLK and FSYNC lines are
 * both connected with CODEC and a single master is needed to provide
 * consistent CLK and FSYNC to slaves, hence configuring SoC TERT TDM as
 * single master and bring up a dummy hostless from TERT to SEC to ensure
 * both slave SoC SEC TDM and CODEC are driven upon system boot.
 */
int32_t auto_audio_ext_enable_hostless()
{
    int32_t ret = 0;
    int i = 0, j = 0, k = 0;
    int fd = 0;
    int sleepRetry = 0;
    char buf[8];
    int count = 0;
    char fn[256];
    struct pcm_config pcm_config = {
        .channels = 1,
        .rate = 48000,
        .period_size = 240,
        .period_count = 2,
        .format = PCM_FORMAT_S16_LE,
        .start_threshold = 0,
        .stop_threshold = INT_MAX,
        .avail_min = 0,
    };

    pthread_mutex_lock(&info->lock);

    place_marker("ES enable hostless start");
    freopen("/dev/kmsg", "w", stdout);
    printf("Enable ES hostless start\n ");

    if (!info->mixer) {
        freopen("/dev/kmsg", "w", stdout);
        printf(" mixer is NULL");
        ret = -1;
        goto exit;
    }

    for (i = 0; i < MAX_SESSION; i++) {
        if (info->hostless[i].enable) {
            freopen("/dev/kmsg", "w", stdout);
            printf("hostless is already enabled");
            continue;
        }

	if(i==0) {
		pcm_id[0]=PCM_ID_1_TX;
		pcm_id[1]=PCM_ID_1_RX;

	} else{
		pcm_id[2]=PCM_ID_2_TX;
		pcm_id[3]=PCM_ID_2_RX;
	}
        ret = auto_audio_ext_set_mixer_ctl(info->mixer,
                                        audio_route[i], 1);
        if (ret) {
            freopen("/dev/kmsg", "w", stdout);
	    printf("auto_audio_ext_set_mixer_ctl failed ");
            goto error;
        }

	if(i == 0)
		k = 0;
	else
		k = 2;

	/* Wait till device is created before opening */
	snprintf(fn, sizeof(fn), "/dev/snd/pcmC0D%uc", pcm_id[k]);
again:
	fd = open(fn, O_RDONLY);
	if ((fd < 0) && (sleepRetry < MAX_SLEEP_RETRY)){
		freopen("/dev/kmsg", "w", stdout);
		printf("pcm open device %d failed %d\n",pcm_id[k],sleepRetry );
		AUDIO_CHIME_SLEEP(AUDIO_CHIME_WAIT_TIME * 1000);
		sleepRetry++;
		goto again;
	} else {
		freopen("/dev/kmsg", "w", stdout);
		printf("pcm device %d online for early chime - \n", pcm_id[k]);
		sleepRetry = 0;
		close(fd);
	}

        info->hostless[i].pcm_tx = pcm_open(info->snd_card,
                                        pcm_id[k],
                                        PCM_IN, &pcm_config);
        if (info->hostless[i].pcm_tx &&
            !pcm_is_ready(info->hostless[i].pcm_tx)) {
            freopen("/dev/kmsg", "w", stdout);
	    printf("Failed opening pcm_tx %d %d\n", info->snd_card, pcm_id[k]);
	    ret = -1;
        }
	else {
            freopen("/dev/kmsg", "w", stdout);
	    printf("Opened pcm_tx %d %d\n", info->snd_card, pcm_id[k]);
	}

        info->hostless[i].pcm_rx = pcm_open(info->snd_card,
                                        pcm_id[k+1],
                                        PCM_OUT, &pcm_config);
        if (info->hostless[i].pcm_rx &&
            !pcm_is_ready(info->hostless[i].pcm_rx)) {
            freopen("/dev/kmsg", "w", stdout);
	    printf("Failed opening pcm_rx %d %d\n", info->snd_card, pcm_id[k+1]);
	    ret = -1;
            goto error;
        }
	else {
            freopen("/dev/kmsg", "w", stdout);
	    printf("Opened pcm_rx %d %d\n", info->snd_card, pcm_id[k+1]);
	}

        if (pcm_start(info->hostless[i].pcm_tx) < 0) {
            freopen("/dev/kmsg", "w", stdout);
	    printf("Failed starting pcm_tx");
            ret = -1;
            goto error;
        }
        if (pcm_start(info->hostless[i].pcm_rx) < 0) {
            freopen("/dev/kmsg", "w", stdout);
	    printf("Failed starting pcm_rx");
            ret = -1;
            goto error;
        }

        info->hostless[i].enable = 1;
    }

    place_marker("Completed ES hostless configuration");
    freopen("/dev/kmsg", "w", stdout);
    printf("Completed ES hostless configuration ");
    pthread_mutex_unlock(&info->lock);
    return ret;

error:
    for (j = i; j >= 0; j--) {
        if (info->hostless[j].pcm_rx)
            pcm_close(info->hostless[j].pcm_rx);
        if (info->hostless[j].pcm_tx)
            pcm_close(info->hostless[j].pcm_tx);
        auto_audio_ext_set_mixer_ctl(info->mixer,
                                    audio_route[j], 0);
        info->hostless[j].enable = 0;
    }
exit:
    pthread_mutex_unlock(&info->lock);
    place_marker("AudioD Failed");
    freopen("/dev/kmsg", "w", stdout);
    printf("AudioD Failed ");
    return ret;
}

int32_t auto_audio_ext_set_snd_card(int snd_card)
{
    int32_t ret = 0;
    struct stat st = {0};
    char buf[32];
    int fd1,fd = 0;
    int sleepRetry = 0;
    freopen("/dev/kmsg", "w", stdout);
    printf("auto_audio_ext_set_snd_card start\n");

    info = (struct snd_card_info *)calloc(1, sizeof(struct snd_card_info));
    if (info == NULL) {
        freopen("/dev/kmsg", "w", stdout);
        printf("Memory allocation failed");
        return -1;
    }
    memset(info, 0, sizeof(struct snd_card_info));
    info->snd_card = snd_card;

again:
    fd = open("/sys/class/sound/card0/id", O_RDONLY);
    if ((fd < 0) && (sleepRetry < MAX_SLEEP_RETRY)){
        freopen("/dev/kmsg", "w", stdout);
	printf("Open card id failed %d\n",sleepRetry);
        AUDIO_CHIME_SLEEP(AUDIO_CHIME_WAIT_TIME * 1000);
        sleepRetry++;
        goto again;
    } else if(-1 == read(fd, buf, sizeof(buf))) {
        freopen("/dev/kmsg", "w", stdout);
	printf("Read failed");
        return -1;
    } else {
        freopen("/dev/kmsg", "w", stdout);
	printf("Card0 online for early chime - %s\n",buf);
    }
    if (info->mixer) {
        freopen("/dev/kmsg", "w", stdout);
        printf("Closing existing mixer");
        mixer_close(info->mixer);
    }
cardagain:
    fd1 = open("/dev/snd/controlC0", O_RDONLY);
    if ((fd1 < 0) && (sleepRetry < MAX_SLEEP_RETRY)){
        freopen("/dev/kmsg", "w", stdout);
	printf("Open snd card failed %d\n",sleepRetry);
        AUDIO_CHIME_SLEEP(AUDIO_CHIME_WAIT_TIME * 1000);
        sleepRetry++;
        goto cardagain;
    }
    close(fd1);
    info->mixer = mixer_open(snd_card);
    if (!info->mixer) {
        freopen("/dev/kmsg", "w", stdout);
        printf("Failed to open mixer");
        ret = -1;
    }

    freopen("/dev/kmsg", "w", stdout);
    printf("auto_audio_ext_set_snd_card end\n");
    return ret;
}

