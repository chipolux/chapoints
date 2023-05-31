#ifndef SMOKEMACHINEMANAGER_H
#define SMOKEMACHINEMANAGER_H

#include <QObject>
#include <QtQml>

#include "qtutils.h"

class SmokeMachineManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Backend only.")

  public:
    // for checking status
    inline const static QString PingUrl{u"http://%1/"_qs};
    // for triggering smoke
    inline const static QString ActivateUrl{u"http://%1/activate?duration=%2"_qs};

    explicit SmokeMachineManager(QObject *parent = nullptr);

  public slots:
    void activate();

  private slots:
    void ping();
    void pingFinished();
    void activateFinished();

  private:
    QNetworkAccessManager *m_nam;
    QTimer *m_pingTimer;

    RO_PROP(bool, online, setOnline)
    RW_PROP(QString, ipAddress, setIpAddress)
    RW_PROP(int, duration, setDuration)
};

#endif // SMOKEMACHINEMANAGER_H
