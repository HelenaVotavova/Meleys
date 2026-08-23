#include <inkview.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define ROOT "/mnt/ext1/Podcasts"
#define LOG ROOT "/audio-ipc-probe.txt"
#define MAX_LINES 400
#define PAGE_LINES 34

static char lines[MAX_LINES][180];
static int count, page;
static ifont *title_font, *body_font;

static void collect(void) {
  const char *cmd =
    "mkdir -p '" ROOT "'; { "
    "echo '=== COMMANDS ==='; command -v dbus-send; command -v busctl; command -v strings; "
    "echo '=== AUDIO PROCESSES ==='; ps w | grep -Ei 'audio|music|player|mpd' | grep -v grep; "
    "echo '=== DBUS SYSTEM ==='; dbus-send --system --print-reply=literal --dest=org.freedesktop.DBus / org.freedesktop.DBus.ListNames; "
    "echo '=== DBUS SESSION ==='; dbus-send --session --print-reply=literal --dest=org.freedesktop.DBus / org.freedesktop.DBus.ListNames; "
    "echo '=== UNIX SOCKETS ==='; cat /proc/net/unix | grep -Ei 'dbus|audio|music|player|mpd'; "
    "echo '=== BINARY STRINGS ==='; "
    "for f in /ebrmain/bin/music /ebrmain/cramfs/bin/music /ebrmain/cramfs/bin/music_player.app /ebrmain/cramfs/bin/audio_initializer /ebrmain/cramfs/bin/audio_book.app /ebrmain/cramfs/bin/audio_books.app; do "
    "[ -r \"$f\" ] || continue; echo \"--- $f\"; strings \"$f\" 2>/dev/null | grep -Ei 'org\\.|dbus|mpris|interface|service|playback|playlist' | head -60; done; "
    "} >'" LOG "' 2>&1";
  FILE *f;
  mkdir(ROOT, 0777);
  system(cmd);
  f = fopen(LOG, "r");
  count = 0;
  if (!f) { snprintf(lines[count++], sizeof lines[0], "Log nelze otevrit."); return; }
  while (count < MAX_LINES && fgets(lines[count], sizeof lines[0], f)) {
    lines[count][strcspn(lines[count], "\r\n")] = 0;
    count++;
  }
  fclose(f);
}

static void draw(void) {
  int i, start = page * PAGE_LINES;
  char header[100];
  ClearScreen();
  SetFont(title_font, BLACK);
  snprintf(header, sizeof header, "Audio IPC diagnostika  %d/%d", page + 1,
           (count + PAGE_LINES - 1) / PAGE_LINES);
  DrawTextRect(30, 25, ScreenWidth() - 60, 50, header, ALIGN_LEFT);
  SetFont(body_font, BLACK);
  for (i = 0; i < PAGE_LINES && start + i < count; ++i)
    DrawTextRect(30, 85 + i * 34, ScreenWidth() - 60, 32, lines[start + i], ALIGN_LEFT | DOTS);
  DrawTextRect(30, ScreenHeight() - 70, ScreenWidth() - 60, 40,
               "Vlevo: zpet     Vpravo: dalsi", ALIGN_CENTER);
  FullUpdate();
}

static int handler(int type, int p1, int p2) {
  int pages = (count + PAGE_LINES - 1) / PAGE_LINES;
  if (type == EVT_INIT) {
    title_font = OpenFont(DEFAULTFONTB, 30, 1);
    body_font = OpenFont(DEFAULTFONT, 19, 1);
    collect();
    draw();
  } else if (type == EVT_REPAINT) draw();
  else if (type == EVT_POINTERUP) {
    if (p1 < ScreenWidth() / 2 && page > 0) page--;
    else if (p1 >= ScreenWidth() / 2 && page + 1 < pages) page++;
    draw();
  } else if (type == EVT_KEYDOWN && p1 == IV_KEY_BACK) CloseApp();
  else if (type == EVT_EXIT) { CloseFont(title_font); CloseFont(body_font); }
  return 0;
}

int main(void) { InkViewMain(handler); return 0; }

