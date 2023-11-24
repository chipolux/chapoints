#include "smokemachinemanager.h"

#include <QNetworkAccessManager>

SmokeMachineManager::SmokeMachineManager(QObject *parent)
    : QObject{parent}
    , m_nam(new QNetworkAccessManager(this))
    , m_pingTimer(new QTimer(this))
    , m_online(false)
    , m_ipAddress(u"192.168.1.224"_qs)
    , m_duration(10) // seconds
{
    // periodically check that the collar is still online
    connect(m_pingTimer, &QTimer::timeout, this, &SmokeMachineManager::ping);
    m_pingTimer->setSingleShot(false);
    m_pingTimer->setInterval(10 * 1000);
    m_pingTimer->start();
    QTimer::singleShot(5000, this, &SmokeMachineManager::ping);
}

void SmokeMachineManager::ping()
{
    QNetworkRequest request(PingUrl.arg(m_ipAddress));
    QNetworkReply *reply = m_nam->get(request);
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &SmokeMachineManager::pingFinished);
}

void SmokeMachineManager::pingFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(QObject::sender());
    const bool online = reply->error() == QNetworkReply::NoError;
    if (!m_online && online) {
        qInfo() << "Smoke machine came online!";
    }
    if (m_online && !online) {
        qWarning() << "Smoke machine went offline!";
    }
    setOnline(online);
}

void SmokeMachineManager::activate()
{
    QNetworkRequest request(ActivateUrl.arg(m_ipAddress).arg(m_duration));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "text/plain");
    QNetworkReply *reply = m_nam->post(request, "");
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &SmokeMachineManager::activateFinished);
}

void SmokeMachineManager::activateFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(QObject::sender());
    if (reply->error() == QNetworkReply::NoError) {
        qInfo() << "Smoke active!";
    } else {
        qWarning() << "Failed to activate smoke!";
    }
}
