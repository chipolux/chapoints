#include "twitchmanager.h"

#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QUuid>

#include "secrets.h"

TwitchManager::TwitchManager(QObject *parent)
    : QObject{parent}
    , m_nam(new QNetworkAccessManager(this))
    , m_validateTimer(new QTimer(this))
    , m_expectedState()
    , m_accessToken()
    , m_userId()
    , m_loading(false)
    , m_loggedIn(false)
    , m_autoLogin(false)
    , m_userName()
    , m_rewards()
{
    QSettings settings;
    settings.beginGroup(u"Twitch"_qs);
    m_autoLogin = settings.value(u"AutoLogin"_qs).toBool();
    settings.endGroup();

    // we are required to validate our tokens on startup and every hour while
    // running or risk an audit or throttling
    connect(m_validateTimer, &QTimer::timeout, this, [&]() { validate(); });
    m_validateTimer->setSingleShot(false);
    m_validateTimer->setInterval(1000 * 60 * 60);
    m_validateTimer->start();
    QTimer::singleShot(1000, this, [&]() { validate(); });
}

void TwitchManager::handleCallback(const QUrl &url)
{
    if (m_expectedState.isEmpty()) {
        qWarning() << "Unexpected callback:" << url;
        return;
    }

    const QUrlQuery query(url.query());
    const QString state = query.queryItemValue(u"state"_qs, QUrl::FullyDecoded);
    if (m_expectedState != state) {
        qWarning() << "Ignoring callback with incorrect state:" << url;
        return;
    }
    // we only want to see our expected state once, no re-use!
    m_expectedState.clear();

    const QString error = query.queryItemValue(u"error"_qs, QUrl::FullyDecoded);
    if (!error.isEmpty()) {
        qWarning() << "Login failed:" << error;
        setLoading(false);
        return;
    }

    setLoading(true);
    const QString code = query.queryItemValue(u"code"_qs, QUrl::FullyDecoded);
    QSettings settings;
    settings.remove(u"TwitchSession"_qs);

    qInfo() << "Sending request to authorize session...";
    QNetworkRequest request(TokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_qs);
    QUrlQuery formData;
    formData.addQueryItem(u"client_id"_qs, secrets::twitchClientId);
    formData.addQueryItem(u"client_secret"_qs, secrets::twitchClientSecret);
    formData.addQueryItem(u"code"_qs, code);
    formData.addQueryItem(u"grant_type"_qs, u"authorization_code"_qs);
    formData.addQueryItem(u"redirect_uri"_qs, secrets::twitchRedirctUri);
    QNetworkReply *reply = m_nam->post(request, formData.toString().toLatin1());
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &TwitchManager::authorizeFinished);
}

void TwitchManager::authorizeFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(QObject::sender());

    // early exit for major errors
    if (!reply->isOpen() || reply->error() != QNetworkReply::NoError) {
        QString message = reply->errorString();
        // sometimes twitch will return a 401 with a JSON payload with error info
        if (reply->isOpen()) {
            QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
            message = response.value(u"message"_qs).toString();
        }
        qWarning() << "Authorize failed:" << message;
        updateLoggedIn();
        setLoading(false);
        return;
    }

    QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();

    // check for access and refresh tokens
    const QString accessToken = response.value(u"access_token"_qs).toString();
    const QString refreshToken = response.value(u"refresh_token"_qs).toString();
    if (accessToken.isEmpty() || refreshToken.isEmpty()) {
        qWarning() << "Authorize failed: did not recieve tokens!";
        updateLoggedIn();
        setLoading(false);
        return;
    }

    // check that our scopes match exactly
    QJsonArray scopes = response.value(u"scope"_qs).toArray();
    if (!validateScopes(scopes)) {
        qWarning() << "Authorize failed: scopes do not match!";
        qWarning() << "   Scopes:" << scopes;
        if (m_autoLogin) {
            qInfo() << "Attempting auto login with forced verification...";
            QTimer::singleShot(500, this, [&]() { login(true); });
        } else {
            updateLoggedIn();
            setLoading(false);
        }
        return;
    }

    // if we reach this point then everything should be good!
    QSettings settings;
    settings.beginGroup(u"TwitchSession"_qs);
    settings.setValue(u"AccessToken"_qs, accessToken);
    settings.setValue(u"RefreshToken"_qs, refreshToken);
    qInfo() << "Authorize success!";

    // finally, we want to do a good faith validate
    updateLoggedIn();
    QTimer::singleShot(500, this, [&]() { validate(true); });
}

void TwitchManager::validate(const bool &force)
{
    if (m_loading & !force) {
        qWarning() << "Cannot validate while other actions are in progress!";
        return;
    }

    setLoading(true);
    QSettings settings;
    settings.beginGroup(u"TwitchSession"_qs);
    const QString accessToken = settings.value(u"AccessToken"_qs).toString();
    if (accessToken.isEmpty()) {
        updateLoggedIn();
        if (m_autoLogin) {
            qInfo() << "Attempting auto login...";
            QTimer::singleShot(500, this, [&]() { login(); });
        } else {
            qWarning() << "Validate failed: no session!";
            setLoading(false);
        }
        return;
    } else {
        updateLoggedIn();
    }

    qInfo() << "Sending request to validate tokens...";
    QNetworkRequest request(ValidateUrl);
    request.setRawHeader("Authorization", "Bearer " + accessToken.toLatin1());
    QNetworkReply *reply = m_nam->get(request);
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &TwitchManager::validateFinished);
}

void TwitchManager::validateFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(QObject::sender());

    // early exit for major errors
    if (!reply->isOpen() || reply->error() != QNetworkReply::NoError) {
        QString message = reply->errorString();
        // sometimes twitch will return a 401 with a JSON payload with error info
        if (reply->isOpen() && reply->error() == QNetworkReply::InternalServerError) {
            QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
            message = response.value(u"message"_qs).toString();
        }
        qWarning() << "Validate failed:" << message;
        QTimer::singleShot(500, this, &TwitchManager::refresh);
        return;
    }

    QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();

    // check that our scopes match exactly
    QJsonArray scopes = response.value(u"scopes"_qs).toArray();
    if (!validateScopes(scopes)) {
        qWarning() << "Validate failed: scopes do not match!";
        qWarning() << "   Scopes:" << scopes;
        if (m_autoLogin) {
            qInfo() << "Attempting auto login with forced verification...";
            QTimer::singleShot(500, this, [&]() { login(true); });
        } else {
            qWarning() << "Logging out due to invalid scopes...";
            QTimer::singleShot(500, this, &TwitchManager::logout);
        }
        return;
    }

    // if we reach this point then everything should be good!
    const QString login = response.value(u"login"_qs).toString();
    const QString userId = response.value(u"user_id"_qs).toString();
    QSettings settings;
    settings.beginGroup(u"TwitchSession"_qs);
    settings.setValue(u"Login"_qs, login);
    settings.setValue(u"UserId"_qs, userId);
    updateLoggedIn();
    setLoading(false);
    qInfo() << "Validate success!";
    qDebug() << "  Login:" << login;
    qDebug() << "  UserId:" << userId;
    emit validated();
}

void TwitchManager::refresh()
{
    setLoading(true);
    QSettings settings;
    settings.beginGroup(u"TwitchSession"_qs);
    const QString accessToken = settings.value(u"AccessToken"_qs).toString();
    const QString refreshToken = settings.value(u"RefreshToken"_qs).toString();
    if (accessToken.isEmpty() || refreshToken.isEmpty()) {
        if (m_autoLogin) {
            qInfo() << "Attempting auto login...";
            QTimer::singleShot(500, this, [&]() { login(); });
        } else if (accessToken.isEmpty()) {
            qWarning() << "Refresh failed: no session!";
            setLoading(false);
        } else {
            qWarning() << "Refresh failed: missing refresh token!";
            qWarning() << "Logging out due to missing refresh token...";
            QTimer::singleShot(500, this, &TwitchManager::logout);
        }
        return;
    }

    qInfo() << "Sending request to refresh tokens...";
    QNetworkRequest request(TokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_qs);
    QUrlQuery formData;
    formData.addQueryItem(u"client_id"_qs, secrets::twitchClientId);
    formData.addQueryItem(u"client_secret"_qs, secrets::twitchClientSecret);
    formData.addQueryItem(u"grant_type"_qs, u"refresh_token"_qs);
    formData.addQueryItem(u"refresh_token"_qs, QUrl::toPercentEncoding(refreshToken));
    QNetworkReply *reply = m_nam->post(request, formData.toString().toLatin1());
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &TwitchManager::refreshFinished);
}

void TwitchManager::refreshFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(QObject::sender());

    // early exit for major errors
    if (!reply->isOpen() || reply->error() != QNetworkReply::NoError) {
        QString message = reply->errorString();
        // sometimes twitch will return a 401 with a JSON payload with error info
        if (reply->isOpen()) {
            QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
            message = response.value(u"message"_qs).toString();
        }
        qWarning() << "Refresh failed:" << message;
        if (m_autoLogin) {
            qInfo() << "Attempting auto login...";
            QTimer::singleShot(500, this, [&]() { login(); });
        } else {
            qWarning() << "Logging out due to failed refresh...";
            QTimer::singleShot(500, this, &TwitchManager::logout);
        }
        return;
    }

    QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();

    // check for access and refresh tokens
    const QString accessToken = response.value(u"access_token"_qs).toString();
    const QString refreshToken = response.value(u"refresh_token"_qs).toString();
    if (accessToken.isEmpty() || refreshToken.isEmpty()) {
        qWarning() << "Refresh failed: did not recieve new tokens!";
        if (m_autoLogin) {
            qInfo() << "Attempting auto login...";
            QTimer::singleShot(500, this, [&]() { login(); });
        } else {
            qWarning() << "Logging out due to failed refresh...";
            QTimer::singleShot(500, this, &TwitchManager::logout);
        }
        return;
    }

    // check that our scopes match exactly
    QJsonArray scopes = response.value(u"scope"_qs).toArray();
    if (!validateScopes(scopes)) {
        qWarning() << "Refresh failed: scopes do not match!";
        qWarning() << "   Scopes:" << scopes;
        if (m_autoLogin) {
            qInfo() << "Attempting auto login with forced verification...";
            QTimer::singleShot(500, this, [&]() { login(true); });
        } else {
            qWarning() << "Logging out due to invalid scopes...";
            QTimer::singleShot(500, this, &TwitchManager::logout);
        }
        return;
    }

    // if we reach this point then everything should be good!
    QSettings settings;
    settings.beginGroup(u"TwitchSession"_qs);
    settings.setValue(u"AccessToken"_qs, accessToken);
    settings.setValue(u"RefreshToken"_qs, refreshToken);
    updateLoggedIn();
    setLoading(false);
    qInfo() << "Refresh success!";
}

void TwitchManager::login(const bool &forceVerify)
{
    setLoading(true);
    QUrlQuery query;
    query.addQueryItem(u"client_id"_qs, secrets::twitchClientId);
    query.addQueryItem(u"redirect_uri"_qs, secrets::twitchRedirctUri);
    query.addQueryItem(u"scope"_qs, QUrl::toPercentEncoding(Scopes.join(' ')));

    // force_verify can be set to true to force the user to re-allow the app
    // used mostly if scopes change
    query.addQueryItem(u"force_verify"_qs, forceVerify ? u"true"_qs : u"false"_qs);

    // response_type can be token for implicit grant flow, code for auth grant flow
    // auth grant let's us refresh the access token without requiring re-login
    query.addQueryItem(u"response_type"_qs, u"code"_qs);

    // state is an optional random string that will be returned for us to verify
    m_expectedState = QUuid::createUuid().toString(QUuid::WithoutBraces);
    query.addQueryItem(u"state"_qs, m_expectedState);

    QUrl url(AuthorizeUrl);
    url.setQuery(query);
    QDesktopServices::openUrl(url);
}

void TwitchManager::logout()
{
    if (!m_loggedIn) {
        qDebug() << "Can't log out if we aren't signed in!";
        setLoading(false);
        return;
    }

    setLoading(true);
    QSettings settings;
    settings.beginGroup(u"TwitchSession"_qs);
    const QString accessToken = settings.value(u"AccessToken"_qs).toString();
    settings.endGroup();
    settings.remove(u"TwitchSession"_qs);
    if (accessToken.isEmpty()) {
        qInfo() << "Logged out.";
        updateLoggedIn();
        setLoading(false);
        return;
    }

    qInfo() << "Sending request to revoke tokens...";
    QNetworkRequest request(RevokeUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/x-www-form-urlencoded"_qs);
    QUrlQuery formData;
    formData.addQueryItem(u"client_id"_qs, secrets::twitchClientId);
    formData.addQueryItem(u"token"_qs, accessToken);
    QNetworkReply *reply = m_nam->post(request, formData.toString().toLatin1());
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &TwitchManager::logoutFinished);
}

void TwitchManager::logoutFinished()
{
    // errors don't really matter here since we can't really do anything else
    // other than tell the user and we already removed any session records
    // when the logout was requested
    qInfo() << "Revoke success!";
    updateLoggedIn();
    setLoading(false);
}

void TwitchManager::updateLoggedIn()
{
    QSettings settings;
    settings.beginGroup(u"TwitchSession"_qs);
    m_accessToken = settings.value(u"AccessToken"_qs).toString();
    m_userId = settings.value(u"UserId"_qs).toString();
    setUserName(settings.value(u"Login"_qs).toString());
    setLoggedIn(!m_accessToken.isEmpty() && !m_userId.isEmpty() && !m_userName.isEmpty());
}

bool TwitchManager::validateScopes(const QJsonArray &scopes) const
{
    QList<QString> expectedScopes(Scopes);
    bool scopeMismatch = false;
    for (auto value : scopes) {
        const auto scope = value.toString();
        const auto count = expectedScopes.removeAll(scope);
        // if we remove less than 1 then the session has an extra scope and if
        // we remove more than one then for some reason twitch sent duplicates
        // and in either case we should probably try and get the scopes to match
        if (count != 1) {
            scopeMismatch = true;
            break;
        }
    }
    // if twitch sent back exactly the same scopes we expect then we shouldn't
    // have flagged a mismatch and our expectedScopes should have been emptied
    if (scopeMismatch || !expectedScopes.isEmpty()) {
        return false;
    }
    return true;
}

QNetworkRequest TwitchManager::createRequest(const QUrl &url) const
{
    QNetworkRequest request(url);
    request.setRawHeader("Client-Id", secrets::twitchClientId.toLatin1());
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toLatin1());
    return request;
}

void TwitchManager::getRewards()
{
    // see https://dev.twitch.tv/docs/api/reference/#get-custom-reward
    // broadcaster_id is required, but there are a few optional parameters
    //   id for specific rewards (can specify up to 50 id=123&id=456, etc)
    //   only_manageable_rewards to filter for rewards our client-id can manage
    if (!m_loggedIn) {
        qWarning() << "Cannot get rewards without being logged in!";
        return;
    }

    setLoading(true);
    QUrlQuery query;
    query.addQueryItem(u"broadcaster_id"_qs, m_userId);
    QUrl url(RewardsUrl);
    url.setQuery(query);
    auto request = createRequest(url);
    QNetworkReply *reply = m_nam->get(request);
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &TwitchManager::getRewardsFinished);
}

void TwitchManager::getRewardsFinished()
{
    // https://dev.twitch.tv/docs/api/reference/#get-custom-reward
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(QObject::sender());
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    switch (status) {
    case 200: {
        if (reply->isOpen()) {
            QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
            QJsonArray rewards = response.value(u"data"_qs).toArray();
            qInfo() << "Got rewards:" << rewards.count();
            m_rewards.clear();
            for (auto value : rewards) {
                m_rewards.append(value.toObject().toVariantMap());
            }
            emit rewardsChanged(m_rewards);
        } else {
            qWarning() << "Get Rewards Failed: Could not read reply!";
        }
        break;
    }
    case 400: {
        qWarning() << "Get Rewards Failed: Invalid parameters!";
        break;
    }
    case 401: {
        qWarning() << "Get Rewards Failed: Not authorized!";
        QTimer::singleShot(500, this, &TwitchManager::refresh);
        return; // intentional, we don't want to go loading = false
    }
    case 403: {
        qWarning() << "Get Rewards Failed: Broadcast not partner or affiliate!";
        break;
    }
    case 404: {
        qWarning() << "Get Rewards Failed: No rewards match the id filter!";
        break;
    }
    default: {
        qWarning() << "Get Rewards Failed: Unknown error" << status;
        break;
    }
    }
    setLoading(false);
}

void TwitchManager::createReward(const QVariantMap &data)
{
    // see https://dev.twitch.tv/docs/api/reference/#create-custom-rewards
    if (!m_loggedIn) {
        qWarning() << "Cannot create reward without being logged in!";
        return;
    }

    setLoading(true);
    QUrlQuery query;
    query.addQueryItem(u"broadcaster_id"_qs, m_userId);
    QUrl url(RewardsUrl);
    url.setQuery(query);
    auto request = createRequest(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_qs);
    QNetworkReply *reply = m_nam->post(
        request, QJsonDocument(QJsonObject::fromVariantMap(data)).toJson(QJsonDocument::Compact));
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &TwitchManager::createRewardFinished);
}

void TwitchManager::createRewardFinished()
{
    // see https://dev.twitch.tv/docs/api/reference/#create-custom-rewards
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(QObject::sender());
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    switch (status) {
    case 200: {
        qInfo() << "Reward created!";
        QTimer::singleShot(500, this, &TwitchManager::getRewards);
        return; // intentional, we don't want to go loading = false
    }
    case 400: {
        qWarning() << "Create Reward Failed: Invalid parameters!";
        break;
    }
    case 401: {
        qWarning() << "Create Reward Failed: Not authorized!";
        QTimer::singleShot(500, this, &TwitchManager::refresh);
        return; // intentional, we don't want to go loading = false
    }
    case 403: {
        qWarning() << "Create Reward Failed: Broadcast not partner or affiliate!";
        break;
    }
    default: {
        qWarning() << "Create Reward Failed: Unknown error" << status;
        break;
    }
    }
    setLoading(false);
}

void TwitchManager::updateReward(const QVariantMap &data)
{
    // see https://dev.twitch.tv/docs/api/reference/#create-custom-rewards
    if (!m_loggedIn) {
        qWarning() << "Cannot update reward without being logged in!";
        return;
    }

    QJsonObject reward = QJsonObject::fromVariantMap(data);
    const QString id = reward.take(u"id"_qs).toString();
    if (id.isEmpty()) {
        qWarning() << "Cannot update reward without an id!";
        return;
    }

    setLoading(true);
    QUrlQuery query;
    query.addQueryItem(u"broadcaster_id"_qs, m_userId);
    query.addQueryItem(u"id"_qs, id);
    QUrl url(RewardsUrl);
    url.setQuery(query);
    auto request = createRequest(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_qs);
    QNetworkReply *reply = m_nam->sendCustomRequest(
        request, "PATCH", QJsonDocument(reward).toJson(QJsonDocument::Compact));
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &TwitchManager::updateRewardFinished);
}

void TwitchManager::updateRewardFinished()
{
    // see https://dev.twitch.tv/docs/api/reference/#create-custom-rewards
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(QObject::sender());
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    switch (status) {
    case 200: {
        qInfo() << "Reward updated!";
        QTimer::singleShot(500, this, &TwitchManager::getRewards);
        return; // intentional, we don't want to go loading = false
    }
    case 400: {
        qWarning() << "Update Reward Failed: Invalid parameters!";
        break;
    }
    case 401: {
        qWarning() << "Update Reward Failed: Not authorized!";
        QTimer::singleShot(500, this, &TwitchManager::refresh);
        return; // intentional, we don't want to go loading = false
    }
    case 403: {
        qWarning() << "Update Reward Failed: Broadcast not partner or affiliate!";
        break;
    }
    default: {
        qWarning() << "Update Reward Failed: Unknown error" << status;
        break;
    }
    }
    setLoading(false);
}

void TwitchManager::getRedemptions(const QVariantMap &params)
{
    // see https://dev.twitch.tv/docs/api/reference/#get-custom-reward-redemption
    if (!m_loggedIn) {
        qWarning() << "Cannot get redemptions without being logged in!";
        return;
    }
    if (!params.contains(u"reward_id"_qs)) {
        qWarning() << "Cannot get redemptions without a reward id!";
        return;
    }
    if (!params.contains(u"id"_qs) && !params.contains(u"status"_qs)) {
        qWarning() << "Cannot get redemptions without an id or status filter!";
        return;
    }

    setLoading(true);
    QUrlQuery query;
    query.addQueryItem(u"broadcaster_id"_qs, m_userId);
    for (auto iter = params.cbegin(); iter != params.cend(); ++iter) {
        query.addQueryItem(iter.key(), iter.value().toString());
    }
    QUrl url(RedemptionsUrl);
    url.setQuery(query);
    auto request = createRequest(url);
    QNetworkReply *reply = m_nam->get(request);
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &TwitchManager::getRedemptionsFinished);
}

void TwitchManager::getRedemptionsFinished()
{
    // see https://dev.twitch.tv/docs/api/reference/#get-custom-reward-redemption
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(QObject::sender());
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    switch (status) {
    case 200: {
        if (reply->isOpen()) {
            QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();
            QJsonArray objs = response.value(u"data"_qs).toArray();
            qInfo() << "Got redemptions:" << objs.count();
            QList<QVariantMap> redemptions;
            for (auto value : objs) {
                redemptions.append(value.toObject().toVariantMap());
            }
            emit gotRedemptions(redemptions);
        } else {
            qWarning() << "Get Redemptions Failed: Could not read reply!";
        }
        break;
    }
    case 400: {
        qWarning() << "Get Redemptions Failed: Invalid parameters!";
        break;
    }
    case 401: {
        qWarning() << "Get Redemptions Failed: Not authorized!";
        QTimer::singleShot(500, this, &TwitchManager::refresh);
        return; // intentional, we don't want to go loading = false
    }
    case 403: {
        qWarning() << "Get Redemptions Failed: Broadcast not partner or affiliate!";
        break;
    }
    default: {
        qWarning() << "Get Redemptions Failed: Unknown error" << status;
        break;
    }
    }
    setLoading(false);
}

void TwitchManager::updateRedemption(const QString &rewardId, const QString &id,
                                     const QString &status)
{
    // see https://dev.twitch.tv/docs/api/reference/#update-redemption-status
    if (!m_loggedIn) {
        qWarning() << "Cannot update redemption without being logged in!";
        return;
    }
    if (id.isEmpty() || rewardId.isEmpty()) {
        qWarning() << "Cannot update redemption without an id or reward id!";
        return;
    }

    setLoading(true);
    QUrlQuery query;
    query.addQueryItem(u"broadcaster_id"_qs, m_userId);
    query.addQueryItem(u"id"_qs, id);
    query.addQueryItem(u"reward_id"_qs, rewardId);
    QUrl url(RedemptionsUrl);
    url.setQuery(query);
    auto request = createRequest(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_qs);
    QNetworkReply *reply = m_nam->sendCustomRequest(
        request, "PATCH",
        QJsonDocument(QJsonObject{{u"status"_qs, status}}).toJson(QJsonDocument::Compact));
    QTimer::singleShot(5000, reply, &QNetworkReply::abort); // timeout after 5 seconds
    connect(reply, &QNetworkReply::finished, this, &TwitchManager::updateRedemptionFinished);
}

void TwitchManager::updateRedemptionFinished()
{
    // see https://dev.twitch.tv/docs/api/reference/#update-redemption-status
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(QObject::sender());
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    switch (status) {
    case 200: {
        qInfo() << "Redemption updated!";
        break;
    }
    case 400: {
        qWarning() << "Update Redemption Failed: Invalid parameters!";
        break;
    }
    case 401: {
        qWarning() << "Update Redemption Failed: Not authorized!";
        QTimer::singleShot(500, this, &TwitchManager::refresh);
        return; // intentional, we don't want to go loading = false
    }
    case 403: {
        qWarning() << "Update Redemption Failed: Broadcast not partner or affiliate!";
        break;
    }
    case 404: {
        qWarning() << "Update Redemption Failed: Unknown redemption or status!";
        break;
    }
    default: {
        qWarning() << "Update Reward Failed: Unknown error" << status;
        break;
    }
    }
    setLoading(false);
}
