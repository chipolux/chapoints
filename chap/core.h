#ifndef CORE_H
#define CORE_H

#include <QLocalServer>
#include <QObject>
#include <QtQml>

#include "twitchmanager.h"

class Core : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Backend only.")

  public:
    explicit Core(QObject *parent = nullptr);
    ~Core();
    bool eventFilter(QObject *object, QEvent *event) override;

    TwitchManager *twitch() const { return m_twitchManager; }

  public slots:
    void save();

  private slots:
    void handleCallback(const QUrl &url);
    void newLocalConnection();
    void localConnectionReadyRead();

  private:
    QLocalServer *m_localServer;
    TwitchManager *m_twitchManager;
};

#endif // CORE_H
