#pragma once

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>

class AudioTest : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(qint64 position READ position NOTIFY changed)
  Q_PROPERTY(qint64 duration READ duration NOTIFY changed)

 public:
  explicit AudioTest(QObject *parent = nullptr);
  QString status() const;
  QString error() const;
  qint64 position() const;
  qint64 duration() const;

  Q_INVOKABLE void playWav();
  Q_INVOKABLE void playPodcast();
  Q_INVOKABLE void togglePause();
  Q_INVOKABLE void seekHalf();

 signals:
  void changed();

 private:
  void playPath(const QString &path);
  void createTestWav();
  QMediaPlayer player_;
  QAudioOutput output_;
};

