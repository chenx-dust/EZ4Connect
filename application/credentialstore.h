#ifndef CREDENTIALSTORE_H
#define CREDENTIALSTORE_H

#include <QString>

struct StoredCredentials
{
    QString username;
    QString password;
};

enum class CredentialStoreStatus
{
    Found,
    NotFound,
    Success,
    Error
};

struct CredentialStoreResult
{
    CredentialStoreStatus status = CredentialStoreStatus::Error;
    StoredCredentials credentials;
    QString error;
    int nativeError = 0;
};

class CredentialStore
{
public:
    virtual ~CredentialStore() = default;

    virtual CredentialStoreResult load(const QString &scopeKey) = 0;
    virtual CredentialStoreResult save(
        const QString &scopeKey,
        const StoredCredentials &credentials
    ) = 0;
    virtual CredentialStoreResult remove(const QString &scopeKey) = 0;
};

#endif // CREDENTIALSTORE_H
