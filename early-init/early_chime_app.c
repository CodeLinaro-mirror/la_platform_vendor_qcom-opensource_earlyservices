/**
@file early_chime_app.c
@brief Early chime application
*/
/*
**Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
** Copyright 2011, The Android Open Source Project
**
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of The Android Open Source Project nor the names of
**       its contributors may be used to endorse or promote products derived
**       from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
** DAMAGE.
**/

#include <tinyalsa/asoundlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
//#include <log/log.h>
#include <cutils/list.h>
#include "early_audiod.h"

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

struct riff_wave_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t wave_id;
};

struct chunk_header {
    uint32_t id;
    uint32_t sz;
};

struct chunk_fmt {
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
};

static int play_close = 0;

void play_sample(FILE *file, unsigned int card, unsigned int device, unsigned int channels,
                 unsigned int rate, unsigned int bits, unsigned int period_size,
                 unsigned int period_count);

int early_chime_pb(char *filename, unsigned int card, unsigned int device, unsigned int period_size, unsigned int period_count);

void stream_close(int sig)
{
    /* allow the stream to be closed gracefully */
    signal(sig, SIG_IGN);
    play_close = 1;
}

int check_param(struct pcm_params *params, unsigned int param, unsigned int value,
                 char *param_name, char *param_unit)
{
    unsigned int min;
    unsigned int max;
    int is_within_bounds = 1;

    min = pcm_params_get_min(params, param);
    if (value < min) {
        freopen("/dev/kmsg", "w", stdout);
        printf("%s is %u%s, device only supports >= %u%s\n", param_name, value,
                param_unit, min, param_unit);
        is_within_bounds = 0;
    }

    max = pcm_params_get_max(params, param);
    if (value > max) {
        freopen("/dev/kmsg", "w", stdout);
        printf("%s is %u%s, device only supports <= %u%s\n", param_name, value,
                param_unit, max, param_unit);
        is_within_bounds = 0;
    }

    return is_within_bounds;
}

int sample_is_playable(unsigned int card, unsigned int device, unsigned int channels,
                        unsigned int rate, unsigned int bits, unsigned int period_size,
                        unsigned int period_count)
{
    struct pcm_params *params;
    int can_play = 0, count = 0;

try_again:
    params = pcm_params_get(card, device, PCM_OUT);
    if (params == NULL) {
        freopen("/dev/kmsg", "w", stdout);
        printf("params NULL, unable to open PCM device, %u. count = %d\n", device, count);
        count++;
        if(count < 5){
            usleep(2000); /* sleep for 2ms and try again */
            goto try_again;
        }
    return 0;
    }

    can_play = check_param(params, PCM_PARAM_RATE, rate, "Sample rate", "Hz");
    can_play &= check_param(params, PCM_PARAM_CHANNELS, channels, "Sample", " channels");
    can_play &= check_param(params, PCM_PARAM_SAMPLE_BITS, bits, "Bitrate", " bits");
    can_play &= check_param(params, PCM_PARAM_PERIOD_SIZE, period_size, "Period size", " frames");
    can_play &= check_param(params, PCM_PARAM_PERIODS, period_count, "Period count", " periods");

    pcm_params_free(params);

    return can_play;
}

void place_marker(char const *name)
{
   int fd=open("/sys/kernel/boot_kpi/kpi_values", O_WRONLY);
   if (fd > 0)
   {
       char earlyapp[128] = {0};
       strlcpy(earlyapp, name, sizeof(earlyapp));
       write(fd, earlyapp, strlen(earlyapp));
       close(fd);
   }
}

void play_sample(FILE *file, unsigned int card, unsigned int device, unsigned int channels,
                 unsigned int rate, unsigned int bits, unsigned int period_size,
                 unsigned int period_count)
{
    struct pcm_config config;
    struct pcm *pcm;
    char *buffer;
    int size;
    int num_read;
    static char const *marker = "Early Chime - Writing audio samples...";
    place_marker(marker);

    memset(&config, 0, sizeof(config));
    config.channels = channels;
    config.rate = rate;
    config.period_size = period_size;
    config.period_count = period_count;
    if (bits == 32)
        config.format = PCM_FORMAT_S32_LE;
    else if (bits == 24)
        config.format = PCM_FORMAT_S24_3LE;
    else if (bits == 16)
        config.format = PCM_FORMAT_S16_LE;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;

    if (!sample_is_playable(card, device, channels, rate, bits, period_size, period_count)) {
        return;
    }

    pcm = pcm_open(card, device, PCM_OUT, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        freopen("/dev/kmsg", "w", stdout);
    printf("Unable to open PCM device");
        return;
    }

    size = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
    buffer = malloc(size);
    if (!buffer) {
        freopen("/dev/kmsg", "w", stdout);
        printf("Unable to allocate %d bytes\n");
        free(buffer);
        pcm_close(pcm);
        return;
    }

    freopen("/dev/kmsg", "w", stdout);
    printf("Playing sample: %u ch, %u hz, %u bit\n", channels, rate, bits);

    /* catch ctrl-c to shutdown cleanly */
    signal(SIGINT, stream_close);

    do {
        num_read = fread(buffer, 1, size, file);
        if (num_read > 0) {
            if (pcm_write(pcm, buffer, num_read)) {
                freopen("/dev/kmsg", "w", stdout);
                printf("Error playing sample\n");
                break;
            }
        }
    } while (!play_close && num_read > 0);

    free(buffer);
    pcm_close(pcm);
}

/****************** audio_chime ***********************/

int main(int argc, char **argv)
{
    char *filename;
    int card = 0;
    unsigned int device = 55;
    unsigned int period_size = 1024;
    unsigned int period_count = 4;

    /* Set sound card and establish hostless pcm session */
    set_snd_card_enable_hostless(card);
    /* parse command line arguments */
    filename = argv[1];
    argv += 2;
    while (*argv) {
        if (strcmp(*argv, "-d") == 0) {
            argv++;
            if (*argv)
                device = atoi(*argv);
        }
        if (*argv && strcmp(*argv, "-p") == 0) {
            argv++;
            if (*argv)
                period_size = atoi(*argv);
        }
        if (*argv && strcmp(*argv, "-n") == 0) {
            argv++;
            if (*argv)
                period_count = atoi(*argv);
        }
        if (*argv && strcmp(*argv, "-D") == 0) {
            argv++;
            if (*argv)
                card = atoi(*argv);
        }
        if (*argv)
            argv++;
    }

    early_chime_pb(filename, card, device, period_size, period_count);
    return 0;
}

int early_chime_pb(char *filename, unsigned int card, unsigned int device, unsigned int period_size, unsigned int period_count)
{
    freopen("/dev/kmsg", "w", stdout);
    printf("Starting Early Chime App\n");
    struct mixer *mixer;
    FILE *file;
    struct riff_wave_header riff_wave_header;
    struct chunk_header chunk_header;
    struct chunk_fmt chunk_fmt = {0};
    int more_chunks = 1;
    struct mixer_ctl *ctl;
    char *mixer_str = "TERT_TDM_RX_0 Audio Mixer MultiMedia23";
    const char* error;
    int32_t rc = 0;
    unsigned int sleepRetry = 0;
    int mixerOpenDone = 0;
    int ret = 0;

    int fd;
    freopen("/dev/kmsg", "w", stdout);
    printf("Starting Early Chime App : %d %d\n",card , device);

    while((mixerOpenDone == 0) && (sleepRetry < MAX_SLEEP_RETRY))
    {
        mixer = mixer_open(card);
        if (!mixer) {
           /*Failed to open mixer, wait 10ms, retry*/
            freopen("/dev/kmsg", "w", stdout);
            printf("Failed to open mixer, sleeping for 10ms\n");
            AUDIO_CHIME_SLEEP(AUDIO_CHIME_WAIT_TIME * 1000);
            sleepRetry++;
        }
        else
            mixerOpenDone = 1;
    }

    if(mixerOpenDone == 0)
    {
        freopen("/dev/kmsg", "w", stdout);
        printf("Mixer_open failed");
        return -ENODEV;
    }

    ctl = mixer_get_ctl_by_name(mixer, mixer_str);
    if(ctl == NULL)
    {
        freopen("/dev/kmsg", "w", stdout);
        printf("mixer_get_ctl failed");
        return -1;
    }
    ret = mixer_ctl_set_value(ctl, 0, 1);
    if(ret)
    {
        freopen("/dev/kmsg", "w", stdout);
        printf("mixer_ctl_set_value failed");
        return -1;
    }
    mixer_close(mixer);

    file = fopen(filename, "rb");
    if (!file) {
        freopen("/dev/kmsg", "w", stdout);
        printf("Unable to open file '%s'\n", filename);
        return -1;
    }

    fread(&riff_wave_header, sizeof(riff_wave_header), 1, file);
    if ((riff_wave_header.riff_id != ID_RIFF) ||
        (riff_wave_header.wave_id != ID_WAVE)) {
        freopen("/dev/kmsg", "w", stdout);
        printf("Error: '%s' is not a riff/wave file\n", filename);
        fclose(file);
        return -1;
    }

    do {
        fread(&chunk_header, sizeof(chunk_header), 1, file);

        switch (chunk_header.id) {
        case ID_FMT:
            fread(&chunk_fmt, sizeof(chunk_fmt), 1, file);
            /* If the format header is larger, skip the rest */
            if (chunk_header.sz > sizeof(chunk_fmt))
                fseek(file, chunk_header.sz - sizeof(chunk_fmt), SEEK_CUR);
            break;
        case ID_DATA:
            /* Stop looking for chunks */
            more_chunks = 0;
            break;
        default:
            /* Unknown chunk, skip bytes */
            fseek(file, chunk_header.sz, SEEK_CUR);
        }
    } while (more_chunks);

    /****************** Play sample ****************************/

    play_sample(file, card, device, chunk_fmt.num_channels, chunk_fmt.sample_rate,
                chunk_fmt.bits_per_sample, period_size, period_count);
close_file:
    fclose(file);

    freopen("/dev/kmsg", "w", stdout);
    printf("Early Chime playback END\n");
    return rc;
}
