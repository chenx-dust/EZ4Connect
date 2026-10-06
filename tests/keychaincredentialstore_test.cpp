#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QEventLoop>
#include <QTimer>
#include <QUuid>

#include <qtkeychain/keychain.h>

#include "infrastructure/security/keychaincredentialstore.h"
#include "infrastructure/security/keychaincredentialpayload_p.h"

#ifdef Q_OS_MACOS
#include <QGuiApplication>
#include <Security/Security.h>
#endif

namespace
{
constexpr int skippedTest = 77;

bool requireStore()
{
    return qEnvironmentVariableIntValue("EZ4CONNECT_REQUIRE_CREDENTIAL_STORE_TEST") == 1;
}

bool unavailable(const CredentialStoreResult &result)
{
    return result.status == CredentialStoreStatus::Error
        && (result.nativeError == QKeychain::AccessDenied
            || result.nativeError == QKeychain::AccessDeniedByUser
            || result.nativeError == QKeychain::NoBackendAvailable
            || result.nativeError == QKeychain::NotImplemented);
}

int resultFailure(const CredentialStoreResult &result, const char *description)
{
    if (unavailable(result) && !requireStore())
    {
        qWarning().noquote() << "SKIPPED: synthetic system credential CRUD unavailable:" << result.error;
        return skippedTest;
    }
    qCritical().noquote() << description << result.error;
    return 1;
}

class SyntheticItemCleanup
{
public:
    SyntheticItemCleanup(KeychainCredentialStore &store, const QString &scope)
        : store(store), scope(scope)
    {
    }
    ~SyntheticItemCleanup()
    {
        if (created)
        {
            const auto result = store.remove(scope);
            if (result.status != CredentialStoreStatus::Success
                && result.status != CredentialStoreStatus::NotFound)
            {
                qWarning().noquote() << "Unable to remove the temporary synthetic credential test item:" << result.error;
            }
        }
    }
    bool created = false;

private:
    KeychainCredentialStore &store;
    QString scope;
};

bool payloadTests()
{
    const StoredCredentials unicode {
        QStringLiteral("synthetic-account-测试"),
        QStringLiteral("synthetic-\"password\"-\\-\n-测试")
    };
    const auto roundTrip = KeychainCredentialPayload::decode(KeychainCredentialPayload::encode(unicode));
    if (roundTrip.status != CredentialStoreStatus::Found
        || roundTrip.credentials.username != unicode.username
        || roundTrip.credentials.password != unicode.password)
    {
        qCritical() << "Unicode credential payload round trip failed";
        return false;
    }
    const QList<QByteArray> invalid {
        "invalid JSON", "[]", "null", "{}",
        R"({"version":2,"username":"synthetic","password":"synthetic"})",
        R"({"version":1.5,"username":"synthetic","password":"synthetic"})",
        R"({"version":"1","username":"synthetic","password":"synthetic"})",
        R"({"version":1,"username":"","password":"synthetic"})",
        R"({"version":1,"username":"synthetic","password":""})",
        R"({"version":1,"username":false,"password":"synthetic"})",
        R"({"version":1,"username":"synthetic","password":123})"
    };
    for (const auto &bytes : invalid)
    {
        const auto result = KeychainCredentialPayload::decode(bytes);
        if (result.status != CredentialStoreStatus::Error
            || !result.credentials.username.isEmpty() || !result.credentials.password.isEmpty())
        {
            qCritical() << "Malformed saved credential payload was accepted";
            return false;
        }
    }
    return true;
}

bool writeMalformedSyntheticItem(const QString &scope)
{
    auto job = new QKeychain::WritePasswordJob(
        QStringLiteral("com.github.chenx-dust.EZ4Connect.sso.credentials.v1"), QCoreApplication::instance()
    );
    job->setKey(QString::fromLatin1(QCryptographicHash::hash(scope.toUtf8(), QCryptographicHash::Sha256).toHex()));
    job->setBinaryData("malformed-synthetic-payload");
    job->setInsecureFallback(false);
    job->setAutoDelete(false);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool completed = false;
    QObject::connect(job, &QKeychain::Job::finished, &loop, [&] {
        completed = true;
        loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(60000);
    job->start();
    if (!completed)
    {
        loop.exec();
    }
    if (!completed)
    {
        job->setAutoDelete(true);
        qCritical() << "Synthetic raw credential write timed out";
        return false;
    }
    const bool success = job->error() == QKeychain::NoError;
    delete job;
    return success;
}
}

int main(int argc, char *argv[])
{
#ifdef Q_OS_MACOS
    // QtKeychain delivers Apple Security results on dispatch_get_main_queue.
    // A GUI application installs Qt's Cocoa dispatcher without opening a window.
    QGuiApplication app(argc, argv);
#else
    QCoreApplication app(argc, argv);
#endif
    KeychainCredentialStore store;
    if (!payloadTests()
        || store.save({}, {QStringLiteral("synthetic"), QStringLiteral("synthetic")}).status != CredentialStoreStatus::Error
        || store.save(QStringLiteral("synthetic"), {}).status != CredentialStoreStatus::Error
        || store.load({}).status != CredentialStoreStatus::Error
        || store.remove({}).status != CredentialStoreStatus::Error)
    {
        qCritical() << "Invalid credential operations did not fail closed";
        return 1;
    }
#ifdef Q_OS_MACOS
    // Inspect only keychain availability. Do not unlock it or show an unlock
    // prompt merely to run an automated test.
    SecKeychainRef defaultKeychain = nullptr;
    OSStatus availability = SecKeychainCopyDefault(&defaultKeychain);
    SecKeychainStatus keychainStatus = 0;
    if (availability == errSecSuccess)
    {
        availability = SecKeychainGetStatus(defaultKeychain, &keychainStatus);
    }
    if (defaultKeychain != nullptr)
    {
        CFRelease(defaultKeychain);
    }
    if (availability != errSecSuccess
        || (keychainStatus & kSecUnlockStateStatus) == 0
        || (keychainStatus & kSecReadPermStatus) == 0
        || (keychainStatus & kSecWritePermStatus) == 0)
    {
        qWarning() << "Default macOS Keychain is unavailable, locked, or lacks read/write access; status" << availability;
        return requireStore() ? 1 : skippedTest;
    }
#endif
    // A fresh random account guarantees the test does not query any user item.
    const QString scope = QStringLiteral("ez4connect-synthetic-crud-test:")
        + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const StoredCredentials original {
        QStringLiteral("synthetic-account-测试"),
        QStringLiteral("synthetic-\"password\"-\\-\n-测试")
    };
    const StoredCredentials replacement {
        QStringLiteral("synthetic-replacement"),
        QStringLiteral("synthetic-updated-password")
    };
    SyntheticItemCleanup cleanup(store, scope);

    bool reentrantOperationRejected = false;
    QTimer::singleShot(0, &app, [&] {
        const auto nested = store.remove(scope);
        reentrantOperationRejected = nested.status == CredentialStoreStatus::Error
            && nested.error == QStringLiteral("A system credential operation is still in progress.");
    });
    auto result = store.load(scope);
    if (result.status != CredentialStoreStatus::NotFound)
    {
        return resultFailure(result, "Fresh synthetic scope was not empty:");
    }
    if (!reentrantOperationRejected)
    {
        qCritical() << "Reentrant credential operation was not rejected";
        return 1;
    }
    result = store.save(scope, original);
    if (result.status != CredentialStoreStatus::Success)
    {
        return resultFailure(result, "Unable to save synthetic credentials:");
    }
    cleanup.created = true;
    result = store.load(scope);
    if (result.status != CredentialStoreStatus::Found)
    {
        return resultFailure(result, "Unable to read synthetic credentials:");
    }
    if (result.credentials.username != original.username || result.credentials.password != original.password)
    {
        qCritical() << "Synthetic credential round trip changed the payload";
        return 1;
    }
    result = store.save(scope, replacement);
    if (result.status != CredentialStoreStatus::Success)
    {
        return resultFailure(result, "Unable to replace synthetic credentials:");
    }
    result = store.load(scope);
    if (result.status != CredentialStoreStatus::Found)
    {
        return resultFailure(result, "Unable to read replaced synthetic credentials:");
    }
    if (result.credentials.username != replacement.username || result.credentials.password != replacement.password)
    {
        qCritical() << "Synthetic credential replacement did not persist";
        return 1;
    }
    if (!writeMalformedSyntheticItem(scope))
    {
        qCritical() << "Unable to inject malformed synthetic credential data";
        return 1;
    }
    result = store.load(scope);
    if (result.status != CredentialStoreStatus::Error
        || !result.credentials.username.isEmpty() || !result.credentials.password.isEmpty())
    {
        qCritical() << "Malformed native credential data was accepted";
        return 1;
    }
    result = store.remove(scope);
    if (result.status != CredentialStoreStatus::Success)
    {
        return resultFailure(result, "Unable to delete synthetic credentials:");
    }
    cleanup.created = false;
    result = store.load(scope);
    if (result.status != CredentialStoreStatus::NotFound)
    {
        return resultFailure(result, "Deleted synthetic credentials remained readable:");
    }
    result = store.remove(scope);
    // Secret Service clear is idempotent and reports success for absent items.
    if (result.status != CredentialStoreStatus::NotFound && result.status != CredentialStoreStatus::Success)
    {
        return resultFailure(result, "Deleting an absent synthetic item failed:");
    }
    qInfo() << "Synthetic system credential save/read/update/malformed/delete and busy-guard tests passed; test item removed";
    return 0;
}
