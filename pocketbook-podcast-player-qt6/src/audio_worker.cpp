#include <QAudioOutput>
#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>
#include <QtMath>

#include <cstdio>

namespace {
const QString kDir = QStringLiteral("/mnt/ext1/Podcasts");
const QString kWav = kDir + QStringLiteral("/HelcinyWorkerTest.wav");
const QString kMp3 = kDir + QStringLiteral("/HelcinyPodcasty.mp3");

void makeWav() {
  QDir().mkpath(kDir);
  QFile file(kWav);
  if (!file.open(QIODevice::WriteOnly)) return;
  constexpr quint32 rate = 22050, samples = rate * 2, bytes = samples * 2;
  QDataStream out(&file);
  out.setByteOrder(QDataStream::LittleEndian);
  out.writeRawData("RIFF", 4); out << quint32(36 + bytes);
  out.writeRawData("WAVEfmt ", 8); out << quint32(16) << quint16(1) << quint16(1);
  out << rate << quint32(rate * 2) << quint16(2) << quint16(16);
  out.writeRawData("data", 4); out << bytes;
  for (quint32 i = 0; i < samples; ++i)
    out << qint16(qSin(2.0 * M_PI * 440.0 * i / rate) * 7000.0);
}
}

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  makeWav();
  QMediaPlayer player;
  QAudioOutput output;
  output.setVolume(0.7f);
  player.setAudioOutput(&output);
  QObject::connect(&player, &QMediaPlayer::playbackStateChanged, [](auto state) {
    std::fprintf(stderr, "playbackState=%d\n", int(state)); std::fflush(stderr);
  });
  QObject::connect(&player, &QMediaPlayer::mediaStatusChanged, [](auto status) {
    std::fprintf(stderr, "mediaStatus=%d\n", int(status)); std::fflush(stderr);
  });
  QObject::connect(&player, &QMediaPlayer::errorChanged, [&player]() {
    std::fprintf(stderr, "error=%s\n", player.errorString().toUtf8().constData());
    std::fflush(stderr);
  });
  std::fprintf(stderr, "TEST 1: WAV\n"); std::fflush(stderr);
  player.setSource(QUrl::fromLocalFile(kWav));
  player.play();
  QTimer::singleShot(4000, [&player]() {
    std::fprintf(stderr, "TEST 2: MP3\n"); std::fflush(stderr);
    player.stop();
    player.setSource(QUrl::fromLocalFile(kMp3));
    player.play();
  });
  QTimer::singleShot(14000, &app, &QCoreApplication::quit);
  return app.exec();
}

