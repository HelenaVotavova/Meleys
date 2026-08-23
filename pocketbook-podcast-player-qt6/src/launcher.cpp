#include <inkview.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace {
char report[1800] = "Spoustim Qt audio test...";

int handler(int type, int key, int) {
  if (type == EVT_INIT || type == EVT_REPAINT) {
    ClearScreen();
    ifont *title = OpenFont(DEFAULTFONTB, 34, 1);
    ifont *body = OpenFont(DEFAULTFONT, 23, 1);
    SetFont(title, BLACK);
    DrawTextRect(35, 30, ScreenWidth() - 70, 55, "Qt audio diagnostika", ALIGN_LEFT);
    SetFont(body, BLACK);
    DrawTextRect(35, 105, ScreenWidth() - 70, ScreenHeight() - 150, report,
                 ALIGN_LEFT | VALIGN_TOP);
    FullUpdate();
    CloseFont(title);
    CloseFont(body);
  } else if (type == EVT_KEYDOWN && key == IV_KEY_BACK) {
    CloseApp();
  }
  return 0;
}
}

int main(int argc, char **argv) {
  std::string self = argc > 0 ? argv[0] : "/mnt/ext1/applications/HelcinyPodcastyQt.app";
  const auto slash = self.find_last_of('/');
  const std::string dir = slash == std::string::npos ? "." : self.substr(0, slash);
  std::string binary = dir + "/HelcinyPodcastyQt.bin";
  if (access(binary.c_str(), R_OK) != 0) {
    binary = "/mnt/ext1/applications/HelcinyPodcastyQt.bin";
  }
  const char *log = "/mnt/ext1/Podcasts/qt-audio-loader.log";
  std::string command = "QT_DEBUG_PLUGINS=1 '" + binary + "' >'" + log + "' 2>&1";
  const int rc = std::system(command.c_str());
  std::snprintf(report, sizeof(report), "Qt proces skoncil, kod: %d\nSoubor: %s\n\n",
                rc, binary.c_str());
  FILE *file = std::fopen(log, "r");
  if (file) {
    const size_t used = std::strlen(report);
    std::fread(report + used, 1, sizeof(report) - used - 1, file);
    report[sizeof(report) - 1] = 0;
    std::fclose(file);
  } else {
    std::strncat(report, "Log se nepodarilo otevrit.", sizeof(report) - std::strlen(report) - 1);
  }
  InkViewMain(handler);
  return 0;
}
