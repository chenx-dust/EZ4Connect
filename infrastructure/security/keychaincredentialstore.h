#ifndef KEYCHAINCREDENTIALSTORE_H
#define KEYCHAINCREDENTIALSTORE_H

#include "application/credentialstore.h"

#include <QPointer>

namespace QKeychain
{
class Job;
}

class KeychainCredentialStore final : public CredentialStore
{
public:
    CredentialStoreResult load(const QString &scopeKey) override;
    CredentialStoreResult save(
        const QString &scopeKey,
        const StoredCredentials &credentials
    ) override;
    CredentialStoreResult remove(const QString &scopeKey) override;

private:
    // A timed-out asynchronous native request can still be completing.
    QPointer<QKeychain::Job> activeJob;

    CredentialStoreResult runJob(QKeychain::Job *job, bool &pending);
};

#endif // KEYCHAINCREDENTIALSTORE_H
