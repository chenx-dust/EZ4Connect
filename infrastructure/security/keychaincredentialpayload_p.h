#ifndef KEYCHAINCREDENTIALPAYLOAD_P_H
#define KEYCHAINCREDENTIALPAYLOAD_P_H

#include "application/credentialstore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

// Private, versioned payload for the system credential adapter. No profile
// setting contains this payload.
namespace KeychainCredentialPayload
{
inline QByteArray encode(const StoredCredentials &credentials)
{
    return QJsonDocument(QJsonObject {
        {QStringLiteral("version"), 1},
        {QStringLiteral("username"), credentials.username},
        {QStringLiteral("password"), credentials.password}
    }).toJson(QJsonDocument::Compact);
}

inline CredentialStoreResult decode(const QByteArray &bytes)
{
    QJsonParseError parseError;
    const QJsonDocument payload = QJsonDocument::fromJson(bytes, &parseError);
    const QJsonObject object = payload.object();
    const QJsonValue version = object.value(QStringLiteral("version"));
    const QJsonValue username = object.value(QStringLiteral("username"));
    const QJsonValue password = object.value(QStringLiteral("password"));
    if (parseError.error != QJsonParseError::NoError
        || !payload.isObject()
        || !version.isDouble() || version.toDouble() != 1.0
        || !username.isString() || username.toString().isEmpty()
        || !password.isString() || password.toString().isEmpty())
    {
        CredentialStoreResult result;
        result.error = QStringLiteral("The saved credential data is invalid.");
        return result;
    }

    CredentialStoreResult result;
    result.status = CredentialStoreStatus::Found;
    result.credentials = {username.toString(), password.toString()};
    return result;
}
}

#endif // KEYCHAINCREDENTIALPAYLOAD_P_H
