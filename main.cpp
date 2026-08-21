#include "AppController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("TeknoKonsole"));
    app.setApplicationDisplayName(QStringLiteral("TeknoKonsole"));
    app.setOrganizationName(QStringLiteral("TeknoKonsole"));
    app.setDesktopFileName(QStringLiteral("tekno-konsole"));

    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    AppController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);

    const QUrl url(QStringLiteral("qrc:/TeknoKonsole/qml/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(EXIT_FAILURE); },
        Qt::QueuedConnection);

    engine.load(url);
    return app.exec();
}
