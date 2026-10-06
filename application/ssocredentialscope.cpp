#include "ssocredentialscope.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

QString SsoCredentialScope::httpsOrigin(const QUrl &url)
{
    if (!url.isValid()
        || url.isRelative()
        || url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) != 0
        || url.host().isEmpty()
        || url.authority(QUrl::FullyEncoded).contains(QLatin1Char('@')))
    {
        return {};
    }

    const int effectivePort = url.port(443);
    if (effectivePort < 1 || effectivePort > 65535)
    {
        return {};
    }

    QUrl origin;
    origin.setScheme(QStringLiteral("https"));
    origin.setHost(url.host().toLower());
    if (effectivePort != 443)
    {
        origin.setPort(effectivePort);
    }
    return origin.toString(QUrl::FullyEncoded);
}

QString SsoCredentialScope::key(
    const QString &configurationFileName,
    const QUrl &vpnServerUrl,
    const QUrl &ssoPageUrl
)
{
    const QString vpnOrigin = httpsOrigin(vpnServerUrl);
    const QString ssoOrigin = httpsOrigin(ssoPageUrl);
    if (configurationFileName.isEmpty() || vpnOrigin.isEmpty() || ssoOrigin.isEmpty())
    {
        return {};
    }

    const QString configurationPath = QDir::cleanPath(
        QFileInfo(configurationFileName).absoluteFilePath()
    );
    const QJsonArray components {
        QStringLiteral("ez4connect-sso-v1"),
        configurationPath,
        vpnOrigin,
        ssoOrigin
    };
    return QString::fromUtf8(QJsonDocument(components).toJson(QJsonDocument::Compact));
}
