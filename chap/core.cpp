#include "core.h"

#include <QDesktopServices>
#include <QFileOpenEvent>
#include <QSettings>

#define URL_SCHEME u"chap"_qs

Core::Core(QObject *parent)
    : QObject{parent}
    , m_localServer(new QLocalServer(this))
    , m_twitchManager(new TwitchManager(this))
{
    // setup handler so we can get route callbacks
    parent->installEventFilter(this);
    QDesktopServices::setUrlHandler(URL_SCHEME, this, "handleCallback");
#ifdef Q_OS_WIN
    // on windows we add/update the registry to ensure we get callbacks
    const QString appPath = QDir::toNativeSeparators(qApp->applicationFilePath());
    const QString libPath =
        QDir::toNativeSeparators(QLibraryInfo::path(QLibraryInfo::BinariesPath));
    const QString execCmd =
        u"CMD /C \"START \"ChaP\" /D \"%1\" \"%2\""_qs.arg(libPath, appPath) + u" \"%1\""_qs;
    QSettings reg(u"HKEY_CURRENT_USER\\Software\\Classes"_qs, QSettings::NativeFormat);
    reg.beginGroup(URL_SCHEME);
    reg.setValue(u"Default"_qs, u"URL:ChaP Protocol"_qs);
    reg.setValue(u"DefaultIcon/Default"_qs, appPath);
    reg.setValue(u"URL Protocol"_qs, u""_qs);
    reg.setValue(u"shell/open/command/Default"_qs, execCmd);
    reg.endGroup();
    qDebug() << "Registerd handler:" << execCmd;
#endif

    // setup local server for ipc (only used on Windows atm)
    m_localServer->listen(u"chap-rpc"_qs);
    connect(m_localServer, &QLocalServer::newConnection, this, &Core::newLocalConnection);
}

Core::~Core()
{
    QDesktopServices::unsetUrlHandler(URL_SCHEME);
#ifdef Q_OS_WIN
    QSettings reg(u"HKEY_CURRENT_USER\\Software\\Classes"_qs, QSettings::NativeFormat);
    reg.remove(URL_SCHEME);
#endif
}

void Core::save() {}

void Core::handleCallback(const QUrl &url)
{
    const QString host(url.host());
    if (host == u"twitch"_qs) {
        m_twitchManager->handleCallback(url);
        return;
    }
    qWarning() << "Failed to handle url:" << url;
}

bool Core::eventFilter(QObject *, QEvent *event)
{
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent *fileEvent = static_cast<QFileOpenEvent *>(event);
        handleCallback(fileEvent->url());
        return true;
    }
    return false;
}

void Core::newLocalConnection()
{
    QLocalSocket *conn = m_localServer->nextPendingConnection();
    connect(conn, &QLocalSocket::disconnected, conn, &QLocalSocket::deleteLater);
    connect(conn, &QLocalSocket::readyRead, this, &Core::localConnectionReadyRead);
}

void Core::localConnectionReadyRead()
{
    QLocalSocket *conn = qobject_cast<QLocalSocket *>(QObject::sender());
    QDataStream stream(conn);
    int argc;
    stream >> argc;
    for (int i = 0; i < argc; i++) {
        char *b;
        uint l;
        stream.readBytes(b, l);
        QString s(b);
        if (s.startsWith(URL_SCHEME)) {
            handleCallback(s);
        }
    }
}
