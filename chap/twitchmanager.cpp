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
    , m_loading(false)
    , m_loggedIn(false)
    , m_autoLogin(false)
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
        setLoggedIn(false);
        setLoading(false);
        return;
    }

    QJsonObject response = QJsonDocument::fromJson(reply->readAll()).object();

    // check for access and refresh tokens
    const QString accessToken = response.value(u"access_token"_qs).toString();
    const QString refreshToken = response.value(u"refresh_token"_qs).toString();
    if (accessToken.isEmpty() || refreshToken.isEmpty()) {
        qWarning() << "Authorize failed: did not recieve tokens!";
        setLoggedIn(false);
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
            setLoggedIn(false);
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
    setLoggedIn(true);
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
        setLoggedIn(false);
        if (m_autoLogin) {
            qInfo() << "Attempting auto login...";
            QTimer::singleShot(500, this, [&]() { login(); });
        } else {
            qWarning() << "Validate failed: no session!";
            setLoading(false);
        }
        return;
    } else {
        setLoggedIn(true);
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
    qInfo() << "Validate success!";
    qDebug() << "  Login:" << login;
    qDebug() << "  UserId:" << userId;
    setLoggedIn(true);
    setLoading(false);
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
    qInfo() << "Refresh success!";
    setLoggedIn(true);
    setLoading(false);
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
        setLoggedIn(false);
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
    setLoggedIn(false);
    setLoading(false);
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
