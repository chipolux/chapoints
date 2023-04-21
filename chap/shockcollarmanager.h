#ifndef SHOCKCOLLARMANAGER_H
#define SHOCKCOLLARMANAGER_H

#include <QObject>
#include <QtQml>

#include "qtutils.h"

class ShockCollarManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Backend only.")

  public:
    // for checking status of collar
    inline const static QString PingUrl{u"http://%1/"_qs};
    // for triggering shock
    inline const static QString ShockUrl{u"http://%1/shock"_qs};

    explicit ShockCollarManager(QObject *parent = nullptr);

  public slots:
    void shock();

  private slots:
    void ping();
    void pingFinished();
    void shockFinished();

  private:
    QNetworkAccessManager *m_nam;
    QTimer *m_pingTimer;

    RO_PROP(bool, online, setOnline)
    RW_PROP(QString, ipAddress, setIpAddress)
};

#endif // SHOCKCOLLARMANAGER_H
