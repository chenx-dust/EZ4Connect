#include "keychaincredentialstore.h"
#include "keychaincredentialpayload_p.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <QSettings>
#endif

#include <qtkeychain/keychain.h>

namespace
{
const QString serviceName = QStringLiteral("com.github.chenx-dust.EZ4Connect.sso.credentials.v1");
constexpr int operationTimeoutMs = 60 * 1000;

CredentialStoreResult errorResult(const QString &message, int nativeError = 0)
{
    CredentialStoreResult result;
    result.error = message;
    result.nativeError = nativeError;
    return result;
}

CredentialStoreResult statusResult(CredentialStoreStatus status)
{
    CredentialStoreResult result;
    result.status = status;
    return result;
}

QString accountKey(const QString &scopeKey)
{
    return QString::fromLatin1(QCryptographicHash::hash(
        scopeKey.toUtf8(), QCryptographicHash::Sha256
    ).toHex());
}

CredentialStoreResult jobResult(const QKeychain::Job &job)
{
    switch (job.error())
    {
    case QKeychain::NoError:
        return statusResult(CredentialStoreStatus::Success);
    case QKeychain::EntryNotFound:
        return statusResult(CredentialStoreStatus::NotFound);
    case QKeychain::AccessDeniedByUser:
        return errorResult(QStringLiteral("System credential access was cancelled."), job.error());
    case QKeychain::AccessDenied:
        return errorResult(QStringLiteral("System credential access was denied."), job.error());
    case QKeychain::NoBackendAvailable:
        return errorResult(QStringLiteral("No system credential store is available."), job.error());
    case QKeychain::NotImplemented:
        return errorResult(QStringLiteral("The system credential store is unsupported."), job.error());
    case QKeychain::CouldNotDeleteEntry:
        return errorResult(QStringLiteral("The system credential entry could not be removed."), job.error());
    case QKeychain::OtherError:
        // Do not propagate arbitrary backend messages or stored data to logs.
        return errorResult(QStringLiteral("The system credential operation failed."), job.error());
    }
    return errorResult(QStringLiteral("The system credential operation failed."), job.error());
}
}

CredentialStoreResult KeychainCredentialStore::runJob(QKeychain::Job *job, bool &pending)
{
    pending = false;
    if (!QCoreApplication::instance()
        || QThread::currentThread() != QCoreApplication::instance()->thread())
    {
        return errorResult(QStringLiteral("Credential operations require the application thread."));
    }
    if (activeJob)
    {
        return errorResult(QStringLiteral("A system credential operation is still in progress."));
    }

    // The application owns unfinished jobs, rather than this adapter or its
    // dialog. libsecret/gnome-keyring callbacks are not safely cancellable:
    // deleting a timed-out job would leave dangling native callback data.
    job->setParent(QCoreApplication::instance());
    job->setAutoDelete(false);
    job->setInsecureFallback(false);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // QtKeychain's KWallet backend also migrates its old plaintext QSettings
    // entries, even with insecure fallback disabled. Give that migration an
    // empty read-only source so this adapter can only read the secure wallet.
    job->setSettings(new QSettings(QStringLiteral("/dev/null"), QSettings::IniFormat, job));
#endif
    activeJob = job;

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool completed = false;
    QObject::connect(job, &QKeychain::Job::finished, &loop, [&loop, &completed] {
        completed = true;
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(operationTimeoutMs);
    job->start();
    if (!completed)
    {
        loop.exec();
    }

    if (!completed)
    {
        // Leave the native request alive until its callback, then release it.
        // A timeout cannot prove whether a write/delete has already happened.
        pending = true;
        job->setAutoDelete(true);
        return errorResult(QStringLiteral(
            "The system credential operation did not finish in time; its result is unconfirmed."
        ));
    }
    timeout.stop();
    return jobResult(*job);
}

CredentialStoreResult KeychainCredentialStore::load(const QString &scopeKey)
{
    if (scopeKey.isEmpty())
    {
        return errorResult(QStringLiteral("The credential scope is empty."));
    }
    auto job = new QKeychain::ReadPasswordJob(serviceName);
    job->setKey(accountKey(scopeKey));
    bool pending = false;
    CredentialStoreResult result = runJob(job, pending);
    if (pending)
    {
        return result;
    }
    if (result.status == CredentialStoreStatus::Success)
    {
        result = KeychainCredentialPayload::decode(job->binaryData());
    }
    delete job;
    return result;
}

CredentialStoreResult KeychainCredentialStore::save(
    const QString &scopeKey,
    const StoredCredentials &credentials
)
{
    if (scopeKey.isEmpty() || credentials.username.isEmpty() || credentials.password.isEmpty())
    {
        return errorResult(QStringLiteral("The credential scope, account, and password are required."));
    }
    auto job = new QKeychain::WritePasswordJob(serviceName);
    job->setKey(accountKey(scopeKey));
    job->setBinaryData(KeychainCredentialPayload::encode(credentials));
    bool pending = false;
    const CredentialStoreResult result = runJob(job, pending);
    if (!pending)
    {
        delete job;
    }
    if (result.status != CredentialStoreStatus::Success)
    {
        return result;
    }

    // Some KWallet responses lose their native DBus error in QtKeychain.
    // Confirm persistence before reporting success to the user.
    const CredentialStoreResult verified = load(scopeKey);
    if (verified.status != CredentialStoreStatus::Found
        || verified.credentials.username != credentials.username
        || verified.credentials.password != credentials.password)
    {
        return errorResult(QStringLiteral("The saved credentials could not be verified; the result is unconfirmed."));
    }
    return statusResult(CredentialStoreStatus::Success);
}

CredentialStoreResult KeychainCredentialStore::remove(const QString &scopeKey)
{
    if (scopeKey.isEmpty())
    {
        return errorResult(QStringLiteral("The credential scope is empty."));
    }
    auto job = new QKeychain::DeletePasswordJob(serviceName);
    job->setKey(accountKey(scopeKey));
    bool pending = false;
    const CredentialStoreResult result = runJob(job, pending);
    if (!pending)
    {
        delete job;
    }
    if (result.status != CredentialStoreStatus::Success)
    {
        return result;
    }

    const CredentialStoreResult verified = load(scopeKey);
    if (verified.status != CredentialStoreStatus::NotFound)
    {
        return errorResult(QStringLiteral("Credential removal could not be verified; the result is unconfirmed."));
    }
    return statusResult(CredentialStoreStatus::Success);
}
