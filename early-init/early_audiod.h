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


/*======================= I N C L U D E S =========================*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

#include <tinyalsa/asoundlib.h>

__BEGIN_DECLS

#define SEC_TDM_RX_HOSTLESS 48
#define TERT_TDM_TX_HOSTLESS 49
#define QUAT_TDM_RX_HOSTLESS 50
#define QUAT_TDM_TX_HOSTLESS 51
#define QUIN_TDM_RX_HOSTLESS 52
#define QUIN_TDM_TX_HOSTLESS 53

#define AUDIO_CHIME_SLEEP(ms) usleep(( uint32_t )ms)
#define AUDIO_CHIME_WAIT_TIME 50   /* in ms */
#define MAX_SLEEP_RETRY 1000

struct hostless_config {
    int enable;
    struct pcm *pcm_tx;
    struct pcm *pcm_rx;
};

enum {
    MERC_SESSION = 0,
    A2B_SESSION,
    MAX_SESSION
};

enum {
    PCM_INPUT = 0,
    PCM_OUTPUT,
    PCM_MAX
};

struct snd_card_info {
    pthread_mutex_t lock;
    int snd_card;
    struct mixer *mixer;
    struct hostless_config hostless[MAX_SESSION];
};

int32_t auto_audio_ext_set_snd_card(int snd_card);
int32_t auto_audio_ext_enable_hostless(void);
void set_snd_card_enable_hostless(int snd_card);

void place_marker(char const *name);

__END_DECLS

