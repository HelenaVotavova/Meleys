#include <alsa/asoundlib.h>
#include <math.h>
#include <mpg123.h>
#include <stdio.h>
#include <stdlib.h>

#define MP3 "/mnt/ext1/Podcasts/HelcinyPodcasty.mp3"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static snd_pcm_t *open_pcm(unsigned rate) {
  snd_pcm_t *pcm = NULL;
  int rc = snd_pcm_open(&pcm, "hw:0,0", SND_PCM_STREAM_PLAYBACK, 0);
  fprintf(stderr, "snd_pcm_open=%d %s\n", rc, rc < 0 ? snd_strerror(rc) : "OK");
  if (rc < 0) return NULL;
  rc = snd_pcm_set_params(pcm, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED,
                          2, rate, 1, 300000);
  fprintf(stderr, "snd_pcm_set_params=%d %s rate=%u\n", rc,
          rc < 0 ? snd_strerror(rc) : "OK", rate);
  if (rc < 0) { snd_pcm_close(pcm); return NULL; }
  return pcm;
}

static int write_frames(snd_pcm_t *pcm, const short *data, size_t frames) {
  while (frames) {
    snd_pcm_sframes_t done = snd_pcm_writei(pcm, data, frames);
    if (done == -EPIPE) { snd_pcm_prepare(pcm); continue; }
    if (done < 0) { fprintf(stderr, "snd_pcm_writei=%ld %s\n", (long)done, snd_strerror(done)); return -1; }
    data += done * 2;
    frames -= (size_t)done;
  }
  return 0;
}

static int play_tone(void) {
  const unsigned rate = 44100, frames = rate * 2;
  short *samples = malloc(frames * 2 * sizeof(short));
  snd_pcm_t *pcm;
  unsigned i;
  if (!samples) return -1;
  for (i = 0; i < frames; ++i) {
    short value = (short)(sin(2.0 * M_PI * 440.0 * i / rate) * 6000.0);
    samples[i * 2] = value; samples[i * 2 + 1] = value;
  }
  pcm = open_pcm(rate);
  if (!pcm) { free(samples); return -1; }
  fprintf(stderr, "PLAY TONE\n");
  write_frames(pcm, samples, frames);
  snd_pcm_drain(pcm); snd_pcm_close(pcm); free(samples);
  return 0;
}

static int play_mp3(void) {
  mpg123_handle *decoder;
  unsigned char *buffer;
  size_t buffer_size, done;
  long rate; int channels, encoding, err = MPG123_OK;
  snd_pcm_t *pcm;
  mpg123_init();
  decoder = mpg123_new(NULL, &err);
  if (!decoder) return -1;
  mpg123_param(decoder, MPG123_ADD_FLAGS, MPG123_FORCE_STEREO, 0.0);
  if (mpg123_open(decoder, MP3) != MPG123_OK ||
      mpg123_getformat(decoder, &rate, &channels, &encoding) != MPG123_OK) {
    fprintf(stderr, "mpg123 open/format failed: %s\n", mpg123_strerror(decoder));
    mpg123_delete(decoder); mpg123_exit(); return -1;
  }
  mpg123_format_none(decoder);
  mpg123_format(decoder, rate, 2, MPG123_ENC_SIGNED_16);
  fprintf(stderr, "MP3 rate=%ld channels=%d encoding=%d\n", rate, channels, encoding);
  pcm = open_pcm((unsigned)rate);
  if (!pcm) { mpg123_delete(decoder); mpg123_exit(); return -1; }
  buffer_size = mpg123_outblock(decoder);
  buffer = malloc(buffer_size);
  fprintf(stderr, "PLAY MP3\n");
  while (buffer && (err = mpg123_read(decoder, buffer, buffer_size, &done)) != MPG123_DONE) {
    if (err != MPG123_OK && err != MPG123_NEW_FORMAT) break;
    if (done && write_frames(pcm, (short *)buffer, done / 4)) break;
  }
  snd_pcm_drain(pcm); snd_pcm_close(pcm); free(buffer);
  mpg123_close(decoder); mpg123_delete(decoder); mpg123_exit();
  fprintf(stderr, "MP3 finished status=%d\n", err);
  return 0;
}

int main(void) {
  fprintf(stderr, "ALSA WORKER START\n");
  if (play_tone()) return 2;
  return play_mp3() ? 3 : 0;
}
