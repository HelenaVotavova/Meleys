#include "audio_test.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QUrl>
#include <QtMath>

namespace {
const QString kRoot = QStringLiteral("/mnt/ext1/Podcasts");
const QString kWav = kRoot + QStringLiteral("/HelcinyTest.wav");
const QString kPodcast = kRoot + QStringLiteral("/HelcinyPodcasty.mp3");
}

AudioTest::AudioTest(QObject *parent) : QObject(parent) {
  player_.setAudioOutput(&output_);
  output_.setVolume(1.0f);
  connect(&player_, &QMediaPlayer::playbackStateChanged, this, &AudioTest::changed);
  connect(&player_, &QMediaPlayer::mediaStatusChanged, this, &AudioTest::changed);
  connect(&player_, &QMediaPlayer::errorChanged, this, &AudioTest::changed);
  connect(&player_, &QMediaPlayer::positionChanged, this, &AudioTest::changed);
  connect(&player_, &QMediaPlayer::durationChanged, this, &AudioTest::changed);
  createTestWav();
}

QString AudioTest::status() const {
  return QStringLiteral("stav=%1  media=%2").arg(player_.playbackState()).arg(player_.mediaStatus());
}

QString AudioTest::error() const { return player_.errorString(); }
qint64 AudioTest::position() const { return player_.position(); }
qint64 AudioTest::duration() const { return player_.duration(); }

void AudioTest::playPath(const QString &path) {
  player_.stop();
  player_.setSource(QUrl::fromLocalFile(path));
  player_.play();
}

void AudioTest::playWav() { playPath(kWav); }
void AudioTest::playPodcast() { playPath(kPodcast); }
void AudioTest::togglePause() {
  player_.playbackState() == QMediaPlayer::PlayingState ? player_.pause() : player_.play();
}
void AudioTest::seekHalf() { player_.setPosition(player_.duration() / 2); }

void AudioTest::createTestWav() {
  QDir().mkpath(kRoot);
  QFile file(kWav);
  if (!file.open(QIODevice::WriteOnly)) return;
  constexpr quint32 rate = 22050;
  constexpr quint32 samples = rate * 2;
  constexpr quint32 dataSize = samples * 2;
  QDataStream out(&file);
  out.setByteOrder(QDataStream::LittleEndian);
  out.writeRawData("RIFF", 4); out << quint32(36 + dataSize);
  out.writeRawData("WAVEfmt ", 8); out << quint32(16) << quint16(1) << quint16(1);
  out << rate << quint32(rate * 2) << quint16(2) << quint16(16);
  out.writeRawData("data", 4); out << dataSize;
  for (quint32 i = 0; i < samples; ++i) {
    const double fade = 1.0 - static_cast<double>(i) / samples;
    out << qint16(qSin(2.0 * M_PI * 440.0 * i / rate) * 9000.0 * fade);
  }
}

