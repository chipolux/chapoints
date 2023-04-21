#ifndef TWITCHMANAGER_H
#define TWITCHMANAGER_H

#include <QObject>
#include <QtQml>

#include "qtutils.h"

class TwitchManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Backend only.")

  public:
    // for implicit and authorization grant flows
    inline const static QUrl AuthorizeUrl{u"https://id.twitch.tv/oauth2/authorize"_qs};
    // for client credentials grant flow and token refresh
    inline const static QUrl TokenUrl{u"https://id.twitch.tv/oauth2/token"_qs};
    // all integrations must validate their tokens at startup and hourly
    inline const static QUrl ValidateUrl{u"https://id.twitch.tv/oauth2/validate"_qs};
    // for revoking our access to a specific access token (logging out)
    inline const static QUrl RevokeUrl{u"https://id.twitch.tv/oauth2/revoke"_qs};
    // for fetching a list of custom channel rewards
    inline const static QUrl RewardsUrl{
        u"https://api.twitch.tv/helix/channel_points/custom_rewards"_qs};
    // for fetching a list of redemptions for a specific reward
    inline const static QUrl RedemptionsUrl{
        u"https://api.twitch.tv/helix/channel_points/custom_rewards/redemptions"_qs};
    // current scopes we actually use
    inline const static QList<QString> Scopes{u"moderator:manage:announcements"_qs,
                                              u"channel:manage:redemptions"_qs};

    explicit TwitchManager(QObject *parent = nullptr);
    void handleCallback(const QUrl &url);

  signals:
    void validated();

  public slots:
    void refresh();
    void login(const bool &forceVerify = false);
    void logout();

    void getRewards();
    void createReward(const QVariantMap &data);
    void updateReward(const QVariantMap &data);
    void getRedemptions(const QVariantMap &params);
    void updateRedemption(const QString &rewardId, const QString &id, const QString &status);

  private slots:
    void validate(const bool &force = false);
    void validateFinished();
    void authorizeFinished();
    void refreshFinished();
    void logoutFinished();

    void getRewardsFinished();
    void createRewardFinished();
    void updateRewardFinished();
    void getRedemptionsFinished();
    void updateRedemptionFinished();

  private:
    QNetworkAccessManager *m_nam;
    QTimer *m_validateTimer;
    QString m_expectedState;

    QString m_accessToken;
    QString m_userId;

    void updateLoggedIn();
    bool validateScopes(const QJsonArray &scopes) const;
    inline QNetworkRequest createRequest(const QUrl &url) const;

    RO_PROP(bool, loading, setLoading)
    RO_PROP(bool, loggedIn, setLoggedIn)
    RW_PROP(bool, autoLogin, setAutoLogin)

    RO_PROP(QString, userName, setUserName)
    RO_PROP(QList<QVariantMap>, rewards, setRewards)
    RO_PROP(QList<QVariantMap>, redemptions, setRedemptions)
};

#endif // TWITCHMANAGER_H
