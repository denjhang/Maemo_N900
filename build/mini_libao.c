/* mini libao.so.2 v2 - direct ALSA, instrumented */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <alsa/asoundlib.h>

typedef struct ao_device_dummy { int _pad; } ao_device;
typedef struct { int bits; int rate; int channels; int byte_format; } ao_sample_format;
typedef struct ao_option_dummy { int _pad; } ao_option;
typedef struct { char *short_name; char *name; char *comment; char **type; int byte_format; } ao_info;
static snd_pcm_t *pcm = NULL;
static int ao_inited = 0;
static ao_info the_info = { "alsa09", "ALSA mini", "direct ALSA" };

int ao_initialize(void) { ao_inited = 1; return 0; }
void ao_shutdown(void) { ao_inited = 0; }
int ao_driver_id(const char *name) {
    if (!name) return -1;
    if (!strcmp(name, "alsa09") || !strcmp(name, "alsa") || !strcmp(name, "pulse")) return 0;
    return -1;
}
int ao_default_driver_id(void) { return 0; }
ao_info *ao_driver_info(int id) { return (id == 0) ? &the_info : NULL; }

ao_device *ao_open_live(int bits, ao_sample_format *format, ao_option *options) {
    int err; unsigned int rate = format->rate;
    (void)bits; (void)options;
    if (!ao_inited) ao_initialize();
    fprintf(stderr, "mini-ao: open default, %dch %dHz %dbits\n", format->channels, rate, bits);
    err = snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) { fprintf(stderr, "mini-ao: open %s\n", snd_strerror(err)); return NULL; }
    { snd_pcm_hw_params_t *hw; snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(pcm, hw);
    err = snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) { fprintf(stderr, "mini-ao: access %s\n", snd_strerror(err)); goto fail; }
    err = snd_pcm_hw_params_set_format(pcm, hw, SND_PCM_FORMAT_S16_LE);
    if (err < 0) { fprintf(stderr, "mini-ao: format %s\n", snd_strerror(err)); goto fail; }
    err = snd_pcm_hw_params_set_channels(pcm, hw, format->channels);
    if (err < 0) { fprintf(stderr, "mini-ao: channels(%d) %s\n", format->channels, snd_strerror(err)); goto fail; }
    err = snd_pcm_hw_params_set_rate_near(pcm, hw, &rate, 0);
    if (err < 0) { fprintf(stderr, "mini-ao: rate %s\n", snd_strerror(err)); goto fail; }
    err = snd_pcm_hw_params(pcm, hw);
    if (err < 0) { fprintf(stderr, "mini-ao: apply %s\n", snd_strerror(err)); goto fail; }
    { snd_pcm_uframes_t buf = 16384; snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &buf); }
    err = snd_pcm_prepare(pcm);
    if (err < 0) { fprintf(stderr, "mini-ao: prepare %s\n", snd_strerror(err)); goto fail; }
    fprintf(stderr, "mini-ao: OPEN OK %uHz %dch\n", rate, format->channels);
    return (ao_device *)pcm;
    }
fail:
    snd_pcm_close(pcm); pcm = NULL;
    return NULL;
}

int ao_play(ao_device *dev, char *samples, unsigned int num_bytes) {
    snd_pcm_uframes_t frames = num_bytes / 4;
    char *p = samples; snd_pcm_sframes_t w;
    while (frames > 0) {
        w = snd_pcm_writei((snd_pcm_t *)dev, p, frames);
        if (w == -EPIPE) { snd_pcm_prepare((snd_pcm_t *)dev); continue; }
        if (w < 0) return 0;
        p += w * 4; frames -= w;
    }
    return 1;
}

void ao_close(ao_device *dev) {
    if (dev) { snd_pcm_drain((snd_pcm_t *)dev); snd_pcm_close((snd_pcm_t *)dev); }
    pcm = NULL;
}