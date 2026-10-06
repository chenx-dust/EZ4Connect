#include <QApplication>
#include <QDebug>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QVariant>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineView>

#include <functional>
#include <memory>

#include "application/credentialstore.h"
#include "application/ssocredentialscope.h"
#include "presentation/dialogs/ssologinwebview/ssologinwebview.h"

namespace
{
const QUrl pageUrl(QStringLiteral("https://iam.example.test/login"));
const QUrl vpnUrl(QStringLiteral("https://vpn.example.test"));
const QString configurationPath(QStringLiteral("/tmp/ez4connect-synthetic-config.ini"));
const StoredCredentials syntheticCredentials{
    QString::fromUtf8("测试账号-β@example.test"),
    QString::fromUtf8("synthetic-密码-'\"-\\-<>&")
};

class FakeCredentialStore final : public CredentialStore
{
public:
    CredentialStoreResult load(const QString &scope) override
    {
        ++loadCount;
        lastLoadedScope = scope;
        if (duringLoad) duringLoad();
        if (denyLoad) return {CredentialStoreStatus::Error, {}, QStringLiteral("Synthetic denial")};
        if (!hasCredentials) return {CredentialStoreStatus::NotFound};
        return {CredentialStoreStatus::Found, credentials};
    }

    CredentialStoreResult save(const QString &scope, const StoredCredentials &values) override
    {
        ++saveCount;
        savedScope = scope;
        credentials = values;
        hasCredentials = true;
        return {CredentialStoreStatus::Success};
    }

    CredentialStoreResult remove(const QString &scope) override
    {
        ++removeCount;
        removedScope = scope;
        hasCredentials = false;
        credentials = {};
        return {CredentialStoreStatus::Success};
    }

    bool hasCredentials = false;
    bool denyLoad = false;
    int loadCount = 0;
    int saveCount = 0;
    int removeCount = 0;
    QString savedScope;
    QString removedScope;
    QString lastLoadedScope;
    StoredCredentials credentials;
    std::function<void()> duringLoad;
};

class BlockNetwork final : public QWebEngineUrlRequestInterceptor
{
public:
    void interceptRequest(QWebEngineUrlRequestInfo &request) override
    {
        if (request.requestUrl().scheme() == QStringLiteral("http")
            || request.requestUrl().scheme() == QStringLiteral("https"))
            request.block(true);
    }
};

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 5000)
{
    if (condition()) return true;
    QEventLoop loop;
    QTimer poll;
    QTimer timeout;
    poll.setInterval(10);
    timeout.setSingleShot(true);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&]() {
        if (condition()) loop.quit();
    });
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    poll.start();
    timeout.start(timeoutMs);
    loop.exec();
    return condition();
}

QString loginHtml()
{
    return QStringLiteral(R"(
        <!doctype html><html><body>
        <form id='login'>
            <input id='username' autocomplete='username'>
            <input id='password' type='password' autocomplete='current-password'>
            <button type='submit'>Sign in</button>
        </form>
        <script>
            window.submits = 0;
            document.getElementById('login').addEventListener('submit', e => {
                e.preventDefault(); window.submits++;
            });
        </script>
        </body></html>
    )");
}

class Fixture
{
public:
    Fixture()
        : store(std::make_shared<FakeCredentialStore>())
        , dialog(new SsoLoginWebView)
    {
        webView = dialog->findChild<QWebEngineView *>();
        if (!webView) return;
        profile = webView->page()->profile();
        profile->setUrlRequestInterceptor(&networkBlocker);
        dialog->setCredentialStore(store, configurationPath, vpnUrl);
        dialog->resize(1000, 700);
        dialog->show();
    }

    ~Fixture()
    {
        if (profile) profile->setUrlRequestInterceptor(nullptr);
        if (dialog) delete dialog.data();
    }

    bool load(const QString &html = loginHtml(), const QUrl &url = pageUrl)
    {
        if (!dialog || !webView) return false;
        QEventLoop loop;
        QTimer timeout;
        bool completed = false;
        bool succeeded = false;
        timeout.setSingleShot(true);
        QObject::connect(webView, &QWebEngineView::loadFinished, &loop, [&](bool ok) {
            completed = true;
            succeeded = ok;
            loop.quit();
        });
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(10000);
        webView->setHtml(html, url);
        loop.exec();
        if (!completed || !succeeded)
            qCritical() << "Synthetic native SSO document did not finish loading";
        return completed && succeeded;
    }

    bool evaluate(const QString &javascript, QJsonObject &result)
    {
        if (!webView) return false;
        struct Result
        {
            bool completed = false;
            QVariant value;
        };
        const auto state = std::make_shared<Result>();
        QEventLoop loop;
        const QPointer<QEventLoop> guardedLoop(&loop);
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(5000);
        webView->page()->runJavaScript(QStringLiteral("JSON.stringify(") + javascript
                                          + QStringLiteral(")"),
                                      [state, guardedLoop](const QVariant &value) {
                                          state->completed = true;
                                          state->value = value;
                                          if (guardedLoop) guardedLoop->quit();
                                      });
        loop.exec();
        const QJsonDocument document = QJsonDocument::fromJson(state->value.toString().toUtf8());
        if (!state->completed || !document.isObject())
        {
            qCritical() << "Synthetic native SSO JavaScript timed out or failed";
            return false;
        }
        result = document.object();
        return true;
    }

    bool enterCredentials()
    {
        const QJsonObject values{{QStringLiteral("username"), syntheticCredentials.username},
                                 {QStringLiteral("password"), syntheticCredentials.password}};
        QJsonObject result;
        return evaluate(QStringLiteral("(() => { const values = ")
                            + QString::fromUtf8(QJsonDocument(values).toJson(QJsonDocument::Compact))
                            + QStringLiteral("; document.getElementById('username').value = values.username; "
                                             "document.getElementById('password').value = values.password; "
                                             "return {entered: true}; })()"), result);
    }

    QPushButton *button(const QString &text)
    {
        if (!dialog) return nullptr;
        for (auto *candidate : dialog->findChildren<QPushButton *>())
            if (candidate->text() == text) return candidate;
        qCritical() << "Native SSO credential control was missing";
        return nullptr;
    }

    bool statusContains(const QString &text) const
    {
        if (!dialog) return false;
        for (const auto *label : dialog->findChildren<QLabel *>())
            if (label->text().contains(text)) return true;
        return false;
    }

    bool expectDom(const StoredCredentials &credentials, const QString &context)
    {
        QJsonObject values;
        if (!evaluate(QStringLiteral("({username: document.getElementById('username').value, "
                                      "password: document.getElementById('password').value, "
                                      "submits: window.submits})"), values)) return false;
        if (values.value(QStringLiteral("username")).toString() != credentials.username
            || values.value(QStringLiteral("password")).toString() != credentials.password
            || values.value(QStringLiteral("submits")).toInt() != 0)
        {
            qCritical() << context << "did not preserve the expected fields and manual submit";
            return false;
        }
        return true;
    }

    std::shared_ptr<FakeCredentialStore> store;
    QPointer<SsoLoginWebView> dialog;
    QPointer<QWebEngineView> webView;

private:
    BlockNetwork networkBlocker;
    QPointer<QWebEngineProfile> profile;
};

bool savesThenFillsAndForgets()
{
    Fixture fixture;
    if (!fixture.load()
        || !waitUntil([&]() { return fixture.store->loadCount > 0; })
        || !fixture.enterCredentials()) return false;
    QPushButton *save = fixture.button(QString::fromUtf8("保存账号密码"));
    if (!save || !save->isEnabled()) return false;
    save->click();
    if (!waitUntil([&]() { return fixture.store->saveCount == 1; }))
    {
        qCritical() << "Save control did not pass the DOM credentials to the fake store";
        return false;
    }
    const QString expectedScope = SsoCredentialScope::key(configurationPath, vpnUrl, pageUrl);
    if (fixture.store->savedScope != expectedScope
        || fixture.store->credentials.username != syntheticCredentials.username
        || fixture.store->credentials.password != syntheticCredentials.password)
    {
        qCritical() << "Native SSO save used the wrong scope or changed the synthetic credentials";
        return false;
    }

    if (!fixture.load()
        || !waitUntil([&]() { return fixture.statusContains(QString::fromUtf8("已填充保存的账号")); })
        || !fixture.expectDom(syntheticCredentials, QStringLiteral("Next-load autofill")))
        return false;
    QPushButton *remove = fixture.button(QString::fromUtf8("删除本页已保存账号"));
    if (!remove || !remove->isEnabled()) return false;
    remove->click();
    if (fixture.store->removeCount != 1 || fixture.store->hasCredentials
        || fixture.store->removedScope != expectedScope)
    {
        qCritical() << "Delete control did not remove the current scoped account";
        return false;
    }
    if (!fixture.expectDom(syntheticCredentials, QStringLiteral("Delete keeps manual page input")))
        return false;
    const int loadCount = fixture.store->loadCount;
    return fixture.load()
        && waitUntil([&]() { return fixture.store->loadCount > loadCount; })
        && fixture.expectDom({}, QStringLiteral("Deleted account next load"));
}

bool permitsManualLoginWhenStoreDeniesRead()
{
    Fixture fixture;
    fixture.store->denyLoad = true;
    fixture.store->hasCredentials = true;
    fixture.store->credentials = syntheticCredentials;
    if (!fixture.load()
        || !waitUntil([&]() { return fixture.statusContains(QString::fromUtf8("无法读取已保存账号")); })
        || !fixture.expectDom({}, QStringLiteral("Denied store read"))) return false;
    fixture.store->denyLoad = false;
    QPushButton *fill = fixture.button(QString::fromUtf8("填充已保存账号"));
    if (!fill || !fill->isEnabled()) return false;
    fill->click();
    return waitUntil([&]() { return fixture.statusContains(QString::fromUtf8("已填充保存的账号")); })
        && fixture.expectDom(syntheticCredentials, QStringLiteral("Manual retry after denial"));
}

bool refusesHttpWithoutReadingStore()
{
    Fixture fixture;
    if (!fixture.load(loginHtml(), QUrl(QStringLiteral("http://iam.example.test/login"))))
        return false;
    for (const QString &label : {QString::fromUtf8("保存账号密码"),
                                QString::fromUtf8("填充已保存账号"),
                                QString::fromUtf8("删除本页已保存账号")})
    {
        const QPushButton *button = fixture.button(label);
        if (!button || button->isEnabled())
        {
            qCritical() << "Native SSO credential control accepted an HTTP page";
            return false;
        }
    }
    return fixture.store->loadCount == 0 && fixture.store->saveCount == 0
        && fixture.expectDom({}, QStringLiteral("HTTP page"));
}

bool closesSafelyWithPendingScripts()
{
    for (const bool saveOperation : {false, true})
    {
        Fixture fixture;
        if (!fixture.load()
            || !waitUntil([&]() { return fixture.store->loadCount > 0; })
            || !fixture.enterCredentials()) return false;
        if (!saveOperation)
        {
            fixture.store->hasCredentials = true;
            fixture.store->credentials = syntheticCredentials;
        }
        const int loadCount = fixture.store->loadCount;
        QPushButton *button = fixture.button(saveOperation ? QString::fromUtf8("保存账号密码")
                                                          : QString::fromUtf8("填充已保存账号"));
        if (!button) return false;
        button->click();
        fixture.dialog->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        if (!waitUntil([&]() { return fixture.dialog.isNull(); }))
        {
            qCritical() << "Closing the SSO dialog did not destroy its pending-script owner";
            return false;
        }
        // Process renderer completion after the QPointer owner has disappeared.
        QEventLoop drain;
        QTimer::singleShot(100, &drain, &QEventLoop::quit);
        drain.exec();
        if (fixture.store->saveCount != 0 || fixture.store->loadCount != loadCount)
        {
            qCritical() << "A closed native SSO dialog accessed the credential store";
            return false;
        }
    }
    return true;
}

bool closesWhileCredentialReadProcessesEvents()
{
    Fixture fixture;
    fixture.store->hasCredentials = true;
    fixture.store->credentials = syntheticCredentials;
    // Native Keychain authorization can run a nested event loop. Model the
    // dialog disappearing while its synchronous store read is still active.
    fixture.store->duringLoad = [&]() {
        fixture.dialog->close();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    };
    if (!fixture.load() || !waitUntil([&]() { return fixture.dialog.isNull(); }))
    {
        qCritical() << "Native SSO dialog did not close during a reentrant store read";
        return false;
    }
    return fixture.store->loadCount == 1 && fixture.store->saveCount == 0;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    return savesThenFillsAndForgets()
        && permitsManualLoginWhenStoreDeniesRead()
        && refusesHttpWithoutReadingStore()
        && closesSafelyWithPendingScripts()
        && closesWhileCredentialReadProcessesEvents()
        ? 0 : 1;
}
