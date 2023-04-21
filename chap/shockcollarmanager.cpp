#include "shockcollarmanager.h"

#include <QNetworkAccessManager>

ShockCollarManager::ShockCollarManager(QObject *parent)
    : QObject{parent}
    , m_nam(new QNetworkAccessManager(this))
    , m_pingTimer(new QTimer(this))
    , m_online(false)
    , m_ipAddress(u"192.168.1.220"_qs)
{
    // periodically check that the collar is still online
    connect(m_pingTimer, &QTimer::timeout, this, &ShockCollarManager::ping);
    m_pingTimer->setSingleShot(false);
    m_pingTimer->setInterval(10 * 1000);
    m_pingTimer->start();
    QTimer::singleShot(5000, this, &ShockCollarManager::ping);
}

void ShockCollarManager::ping()
{
    QNetworkRequest request(PingUrl.arg(m_ipAddress));
    QNetworkReply *reply = m_nam->get(request);
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &ShockCollarManager::pingFinished);
}

void ShockCollarManager::pingFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(QObject::sender());
    const bool online = reply->error() == QNetworkReply::NoError;
    if (!m_online && online) {
        qInfo() << "Shock collar came online!";
    }
    if (m_online && !online) {
        qWarning() << "Shock collar went offline!";
    }
    setOnline(online);
}

void ShockCollarManager::shock()
{
    QNetworkRequest request(ShockUrl.arg(m_ipAddress));
    QNetworkReply *reply = m_nam->post(request, "");
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &ShockCollarManager::shockFinished);
}

void ShockCollarManager::shockFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(QObject::sender());
    if (reply->error() == QNetworkReply::NoError) {
        qInfo() << "Shock administered!";
    } else {
        qWarning() << "Failed to administer shock!";
    }
}
