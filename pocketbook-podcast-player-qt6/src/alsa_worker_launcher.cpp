#include <inkview.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
char report[2400] = "ALSA worker se spousti...";
int handler(int type, int key, int) {
  if (type == EVT_INIT || type == EVT_REPAINT) {
    ClearScreen();
    ifont *title = OpenFont(DEFAULTFONTB, 34, 1), *body = OpenFont(DEFAULTFONT, 23, 1);
    SetFont(title, BLACK); DrawTextRect(35, 30, ScreenWidth()-70, 55, "ALSA audio worker", ALIGN_LEFT);
    SetFont(body, BLACK); DrawTextRect(35, 105, ScreenWidth()-70, ScreenHeight()-150, report, ALIGN_LEFT|VALIGN_TOP);
    FullUpdate(); CloseFont(title); CloseFont(body);
  } else if (type == EVT_KEYDOWN && key == IV_KEY_BACK) CloseApp();
  return 0;
}
}

int main() {
  const char *root = "/mnt/ext1/applications";
  const char *log = "/mnt/ext1/Podcasts/alsa-worker.log";
  char command[1200];
  std::snprintf(command, sizeof command,
    "{ /ebrmain/bin/audio_initializer; echo initializer_rc=$?; "
    "echo system_alsa:; find /ebrmain /usr/lib /lib -name 'libasound.so*' 2>/dev/null; "
    "LD_LIBRARY_PATH='/ebrmain/lib:/ebrmain/cramfs/lib:/usr/local/lib:/usr/lib:/lib:%s/HelcinyAlsaWorker-libs' "
    "'%s/HelcinyAlsaWorker.bin'; } >'%s' 2>&1",
    root, root, log);
  int rc = std::system(command);
  std::snprintf(report, sizeof report, "Kod: %d\n\nZaznel ton a potom podcast?\n\n", rc);
  FILE *file = std::fopen(log, "r");
  if (file) { size_t used=std::strlen(report); std::fread(report+used,1,sizeof(report)-used-1,file); report[sizeof(report)-1]=0; std::fclose(file); }
  InkViewMain(handler); return 0;
}
