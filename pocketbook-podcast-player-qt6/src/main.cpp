#include <QByteArray>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QUrl>

#include "audio_test.h"

#include <inkview.h>

int main(int argc, char *argv[]) {
  qputenv("QT_PLUGIN_PATH", QByteArray("/ebrmain/plugins"));
  qputenv("QT_QPA_PLATFORM", QByteArray("pocketbook2"));
  QCoreApplication::setSetuidAllowed(true);
  InitInkview(TASK_MAKEACTIVE);
  const int width = ScreenWidth();
  const int height = ScreenHeight() - PanelHeight();
  QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);
  QGuiApplication app(argc, argv);
  const char *font = iv_get_default_font(FONT_FAMILY);
  if (font) QGuiApplication::setFont(QFont(QString::fromUtf8(font), 26));
  AudioTest audio;
  QQmlApplicationEngine engine;
  engine.addImportPath(QStringLiteral("/ebrmain/qml"));
  engine.rootContext()->setContextProperty(QStringLiteral("audioTest"), &audio);
  engine.rootContext()->setContextProperty(QStringLiteral("screenWidth"), width);
  engine.rootContext()->setContextProperty(QStringLiteral("screenHeight"), height);
  engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
  return engine.rootObjects().isEmpty() ? 1 : app.exec();
}

