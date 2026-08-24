#include <inkview.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
char report[2400] = "Audio worker se spousti...";
int handler(int type, int key, int) {
  if (type == EVT_INIT || type == EVT_REPAINT) {
    ClearScreen();
    ifont *title = OpenFont(DEFAULTFONTB, 34, 1);
    ifont *body = OpenFont(DEFAULTFONT, 23, 1);
    SetFont(title, BLACK);
    DrawTextRect(35, 30, ScreenWidth() - 70, 55, "Izolovany audio worker", ALIGN_LEFT);
    SetFont(body, BLACK);
    DrawTextRect(35, 105, ScreenWidth() - 70, ScreenHeight() - 150, report,
                 ALIGN_LEFT | VALIGN_TOP);
    FullUpdate();
    CloseFont(title); CloseFont(body);
  } else if (type == EVT_KEYDOWN && key == IV_KEY_BACK) CloseApp();
  return 0;
}
}

int main() {
  const char *root = "/mnt/ext1/applications";
  const char *log = "/mnt/ext1/Podcasts/audio-worker.log";
  char command[1400];
  std::snprintf(command, sizeof command,
    "{ mkdir -p /mnt/ext1/system/config/helciny-audio-home; "
    "echo 'INITIALIZER'; /ebrmain/bin/audio_initializer; echo initializer_rc=$?; "
    "echo 'AUDIO SOCKETS'; cat /proc/net/unix | grep -Ei 'pulse|audio|music' || true; "
    "echo 'ALSA'; cat /proc/asound/cards 2>&1; ls -la /dev/snd 2>&1; "
    "HOME=/mnt/ext1/system/config/helciny-audio-home "
    "LD_LIBRARY_PATH='%s/HelcinyAudioWorker-libs' "
    "QT_PLUGIN_PATH='%s/HelcinyAudioWorker-plugins' "
    "'%s/HelcinyAudioWorker.bin'; } >'%s' 2>&1", root, root, root, log);
  const int rc = std::system(command);
  std::snprintf(report, sizeof report,
                "Kod: %d\n\nSlyseli jste nejprve ton a potom podcast?\n\n", rc);
  FILE *file = std::fopen(log, "r");
  if (file) {
    const size_t used = std::strlen(report);
    std::fread(report + used, 1, sizeof(report) - used - 1, file);
    report[sizeof(report) - 1] = 0;
    std::fclose(file);
  }
  InkViewMain(handler);
  return 0;
}
