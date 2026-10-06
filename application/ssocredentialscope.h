#ifndef SSOCREDENTIALSCOPE_H
#define SSOCREDENTIALSCOPE_H

#include <QString>
#include <QUrl>

class SsoCredentialScope
{
public:
    // An empty result means that this URL is not eligible for credentials.
    static QString httpsOrigin(const QUrl &url);

    // Paths and query parameters on either URL do not identify a credential.
    // The configuration filename intentionally scopes a locally saved account.
    static QString key(
        const QString &configurationFileName,
        const QUrl &vpnServerUrl,
        const QUrl &ssoPageUrl
    );
};

#endif // SSOCREDENTIALSCOPE_H
