#include <alsa/asoundlib.h>
#include <math.h>
#include <mpg123.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_MP3 "/mnt/ext1/Podcasts/HelcinyPodcasty.mp3"
#define SEEK_FILE "/mnt/ext1/system/config/helciny-podcasty/seek"
#define POSITION_FILE "/mnt/ext1/system/config/helciny-podcasty/position"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Debian armel's mpg123 ABI exports the large-file entry point only. */
extern int mpg123_open_64(mpg123_handle *mh, const char *path);
extern int64_t mpg123_seek_64(mpg123_handle *mh, int64_t sampleoff, int whence);
extern int64_t mpg123_tell_64(mpg123_handle *mh);
extern int64_t mpg123_length_64(mpg123_handle *mh);
static volatile sig_atomic_t stopped;
static volatile sig_atomic_t paused;
static volatile sig_atomic_t volume = 70;

static void handle_signal(int signal) {
  if (signal == SIGTERM || signal == SIGINT) stopped = 1;
  else if (signal == SIGUSR1) paused = !paused;
  else if (signal == SIGUSR2 && volume < 100) volume += 10;
  else if (signal == SIGHUP && volume > 0) volume -= 10;
}

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

static int play_mp3(const char *path) {
  mpg123_handle *decoder;
  unsigned char *buffer;
  size_t buffer_size, done;
  long rate; int channels, encoding, err = MPG123_OK, status_tick = 0;
  int64_t total_samples;
  snd_pcm_t *pcm;
  mpg123_init();
  decoder = mpg123_new(NULL, &err);
  if (!decoder) return -1;
  mpg123_param(decoder, MPG123_ADD_FLAGS, MPG123_FORCE_STEREO, 0.0);
  if (mpg123_open_64(decoder, path) != MPG123_OK ||
      mpg123_getformat(decoder, &rate, &channels, &encoding) != MPG123_OK) {
    fprintf(stderr, "mpg123 open/format failed: %s\n", mpg123_strerror(decoder));
    mpg123_delete(decoder); mpg123_exit(); return -1;
  }
  mpg123_format_none(decoder);
  mpg123_format(decoder, rate, 2, MPG123_ENC_SIGNED_16);
  fprintf(stderr, "Indexing MP3 for seeking\n");
  mpg123_scan(decoder);
  total_samples = mpg123_length_64(decoder);
  fprintf(stderr, "MP3 rate=%ld channels=%d encoding=%d\n", rate, channels, encoding);
  pcm = open_pcm((unsigned)rate);
  if (!pcm) { mpg123_delete(decoder); mpg123_exit(); return -1; }
  buffer_size = mpg123_outblock(decoder);
  buffer = malloc(buffer_size);
  fprintf(stderr, "PLAY MP3\n");
  while (!stopped && buffer && (err = mpg123_read(decoder, buffer, buffer_size, &done)) != MPG123_DONE) {
    size_t i;
    FILE *control;
    if (err != MPG123_OK && err != MPG123_NEW_FORMAT) break;
    while (paused && !stopped) usleep(100000);
    control = fopen(SEEK_FILE, "r");
    if (control) {
      long seconds;
      if (fscanf(control, "%ld", &seconds) == 1 && seconds >= 0) {
        int64_t result = mpg123_seek_64(decoder, (int64_t)seconds * rate, SEEK_SET);
        fprintf(stderr, "seek seconds=%ld result=%lld\n", seconds, (long long)result);
        if (result >= 0) { snd_pcm_drop(pcm); snd_pcm_prepare(pcm); }
      }
      fclose(control); unlink(SEEK_FILE);
    }
    for (i = 0; i + 1 < done; i += 2) {
      short *sample = (short *)(buffer + i);
      *sample = (short)((long)*sample * volume / 100);
    }
    if (done && write_frames(pcm, (short *)buffer, done / 4)) break;
    if (++status_tick >= 40) {
      FILE *position = fopen(POSITION_FILE, "w");
      if (position) {
        fprintf(position, "%ld %ld\n", (long)(mpg123_tell_64(decoder) / rate),
                total_samples > 0 ? (long)(total_samples / rate) : 0L);
        fclose(position);
      }
      status_tick = 0;
    }
  }
  snd_pcm_drain(pcm); snd_pcm_close(pcm); free(buffer);
  mpg123_close(decoder); mpg123_delete(decoder); mpg123_exit();
  fprintf(stderr, "MP3 finished status=%d\n", err);
  unlink(POSITION_FILE); unlink(SEEK_FILE);
  return 0;
}

int main(int argc, char **argv) {
  const char *path = argc > 1 ? argv[1] : DEFAULT_MP3;
  if (argc > 2) {
    int requested = atoi(argv[2]);
    if (requested >= 0 && requested <= 100) volume = requested;
  }
  signal(SIGTERM, handle_signal); signal(SIGINT, handle_signal);
  signal(SIGUSR1, handle_signal); signal(SIGUSR2, handle_signal); signal(SIGHUP, handle_signal);
  fprintf(stderr, "ALSA WORKER START\n");
  if (argc > 1 && !strcmp(argv[1], "--test")) {
    if (play_tone()) return 2;
    path = DEFAULT_MP3;
  }
  return play_mp3(path) ? 3 : (stopped ? 4 : 0);
}
