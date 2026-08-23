#include <inkview.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define AUDIO "/mnt/ext1/Podcasts/HelcinyPodcasty.mp3"
#define LOG "/mnt/ext1/Podcasts/audio-backend-test.txt"

static ifont *title_font, *body_font;
static char status[300] = "1. Nejprve spust AUDIO INITIALIZER.";
static pid_t initializer_pid = -1;

static void log_line(const char *text) {
  FILE *f = fopen(LOG, "a");
  if (f) { fprintf(f, "%s\n", text); fflush(f); fsync(fileno(f)); fclose(f); }
}

static void draw_button(int y, const char *text) {
  DrawRect(55, y, ScreenWidth() - 110, 100, BLACK);
  SetFont(body_font, BLACK);
  DrawTextRect(55, y, ScreenWidth() - 110, 100, text, ALIGN_CENTER | VALIGN_MIDDLE);
}

static void draw(void) {
  ClearScreen();
  SetFont(title_font, BLACK);
  DrawTextRect(45, 35, ScreenWidth() - 90, 60, "Test audio backendu", ALIGN_LEFT);
  SetFont(body_font, BLACK);
  DrawTextRect(50, 110, ScreenWidth() - 100, 100, status, ALIGN_LEFT | VALIGN_TOP);
  draw_button(250, "1  AUDIO INITIALIZER");
  draw_button(390, "2  PLAYFILE");
  draw_button(530, "3  PLAY + ZPOZDENY START");
  DrawTextRect(50, 690, ScreenWidth() - 100, 180,
    "Kroky spoustej postupne. Pokud aplikace spadne, po novem spusteni se zobrazi posledni krok v logu.",
    ALIGN_LEFT | VALIGN_TOP);
  FullUpdate();
}

static void load_last(void) {
  FILE *f = fopen(LOG, "r"); char line[300];
  if (!f) return;
  while (fgets(line, sizeof line, f)) snprintf(status, sizeof status, "Posledni krok: %s", line);
  fclose(f);
}

static void check_initializer(void) {
  int code; pid_t result = waitpid(initializer_pid, &code, WNOHANG);
  if (result == 0) {
    snprintf(status, sizeof status, "audio_initializer zustal spusteny (pid %d).", initializer_pid);
    log_line("01 initializer running");
  } else if (result > 0) {
    snprintf(status, sizeof status, "audio_initializer skoncil, stav %d.", code);
    log_line("01 initializer exited");
  } else {
    snprintf(status, sizeof status, "Stav initializeru nelze zjistit.");
    log_line("01 initializer wait failed");
  }
  draw();
}

static void start_initializer(void) {
  log_line("01 starting audio_initializer");
  initializer_pid = fork();
  if (initializer_pid == 0) {
    execl("/ebrmain/cramfs/bin/audio_initializer", "audio_initializer", (char *)NULL);
    execl("/ebrmain/bin/audio_initializer", "audio_initializer", (char *)NULL);
    _exit(127);
  }
  if (initializer_pid < 0) {
    snprintf(status, sizeof status, "audio_initializer nelze spustit.");
    log_line("01 initializer fork failed");
    draw();
    return;
  }
  snprintf(status, sizeof status, "Cekam na audio_initializer...");
  draw();
  SetWeakTimer("initializer-check", check_initializer, 2000);
}

static void delayed_start(void) {
  log_line("03 calling SetPlayerState PLAYING");
  SetPlayerState(MP_PLAYING);
  log_line("03 SetPlayerState returned");
  snprintf(status, sizeof status, "Pozadavek PLAYING se vratil bez padu.");
  draw();
}

static void test_play(int delayed) {
  log_line(delayed ? "03 calling PlayFile" : "02 calling PlayFile");
  PlayFile(AUDIO);
  log_line(delayed ? "03 PlayFile returned" : "02 PlayFile returned");
  if (delayed) {
    snprintf(status, sizeof status, "PlayFile uspel, za 1 s posilam PLAYING.");
    draw();
    SetWeakTimer("delayed-play", delayed_start, 1000);
  } else {
    snprintf(status, sizeof status, "PlayFile se vratil bez padu.");
    draw();
  }
}

static int handler(int type, int p1, int p2) {
  if (type == EVT_INIT) {
    title_font = OpenFont(DEFAULTFONTB, 38, 1);
    body_font = OpenFont(DEFAULTFONT, 27, 1);
    load_last(); draw();
  } else if (type == EVT_REPAINT) draw();
  else if (type == EVT_POINTERUP) {
    if (p2 >= 230 && p2 < 370) start_initializer();
    else if (p2 >= 370 && p2 < 510) test_play(0);
    else if (p2 >= 510 && p2 < 670) test_play(1);
  } else if (type == EVT_KEYDOWN && p1 == IV_KEY_BACK) CloseApp();
  else if (type == EVT_EXIT) { CloseFont(title_font); CloseFont(body_font); }
  return 0;
}

int main(void) { InkViewMain(handler); return 0; }
