#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "config.h"
#include "core.h"
#include "qtutils.h"

bool registerFonts()
{
    bool success = true;
    QString prefix;

    prefix = u":/chap/resources/Fira_Code/"_qs;
    for (const QString &f : QDir(prefix).entryList({u"*.ttf"_qs})) {
        if (QFontDatabase::addApplicationFont(prefix + f) == -1)
            success = false;
    }

    return success;
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(messageHandler);

#ifdef Q_OS_WIN
    // check for other running instance on windows
    QLocalSocket socket = QLocalSocket();
    socket.connectToServer(u"chap-rpc"_qs);
    if (socket.waitForConnected(1000)) {
        qDebug() << "Connected to other instance, sending arguments:" << argv;
        QByteArray out;
        QDataStream stream(&out, QIODevice::WriteOnly);
        stream << argc;
        for (int i = 0; i < argc; i++) {
            stream << argv[i];
        }
        socket.write(out);
        socket.waitForBytesWritten();
        socket.close();
        return 0;
    }
#endif

    QGuiApplication app(argc, argv);
    app.setApplicationName(APP_NAME);
    app.setOrganizationName(ORG_NAME);
    app.setOrganizationDomain(ORG_DOMAIN);
    app.setApplicationVersion(PROJECT_VER);
    app.setWindowIcon(QIcon(u":/chap/resources/icon.png"_qs));
    QQuickStyle::setStyle(u"Basic"_qs);
    qDebug() << PROJECT_NAME << "version" << PROJECT_VER;

    qDebug() << "Registering custom fonts...";
    if (!registerFonts()) {
        qWarning() << "Failed to register custom fonts!";
    } else {
#ifdef Q_OS_WIN
        app.setFont({u"Fira Code"_qs, 10, QFont::Normal});
#else
        app.setFont({u"Fira Code"_qs, 13, QFont::Normal});
#endif
    }

    qDebug() << "Setting up backend...";
    Core *core = new Core(&app);

    QObject::connect(&app, &QGuiApplication::aboutToQuit, core, [&]() { core->save(); });

    QQmlApplicationEngine engine(&app);
    QQmlContext *ctx = engine.rootContext();
    ctx->setContextProperty(u"core"_qs, core);
    ctx->setContextProperty(u"twitch"_qs, core->twitch());
    ctx->setContextProperty(u"shockCollar"_qs, core->shockCollar());
    ctx->setContextProperty(u"smokeMachine"_qs, core->smokeMachine());

    qDebug() << "Loading QML...";
    const QUrl url(u"qrc:/chap/qml/main.qml"_qs);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreated, &app,
        [url, &app](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl) {
                qCritical("Failed to load any QML objects.");
                app.quit();
            }
        },
        Qt::QueuedConnection);
    engine.load(url);

    qDebug() << "Starting application...";
    return app.exec();
}
