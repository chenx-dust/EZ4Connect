#include "ssologinwebview.h"
#include "ui_ssologinwebview.h"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QToolButton>
#include <QWebEngineHistory>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QtWebEngineCore>
#include <QUrl>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include "application/credentialstore.h"
#include "application/ssocredentialscope.h"

SsoLoginWebView::SsoLoginWebView(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SsoLoginWebView)
{
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);

    setupConnections();
    setupCredentialControls();
}

SsoLoginWebView::~SsoLoginWebView()
{
    ++navigationGeneration;
    if (credentialFillTimer) credentialFillTimer->stop();
    // WebEngine may call outstanding JavaScript callbacks during page destruction.
    // Destroy it while the generation guard and the rest of this class are alive.
    ui->webEngineView->disconnect(this);
    ui->webEngineView->page()->disconnect(this);
    delete ui->webEngineView;
    delete ui;
}

void SsoLoginWebView::setupConnections()
{
    connect(ui->backButton, &QToolButton::clicked, ui->webEngineView, &QWebEngineView::back);
    connect(ui->forwardButton, &QToolButton::clicked, ui->webEngineView, &QWebEngineView::forward);
    connect(ui->reloadButton, &QToolButton::clicked, ui->webEngineView, &QWebEngineView::reload);
    connect(ui->dialogButtonBox, &QDialogButtonBox::rejected, this, [&]()
    {
        if (!loginCompletedEmitted)
        {
            loginCompletedEmitted = true;
            loginCompleted(QString());
        }
    });

    connect(ui->addressLineEdit, &QLineEdit::returnPressed, this, [&]()
    {
        const QUrl target = QUrl::fromUserInput(ui->addressLineEdit->text());
        ui->webEngineView->load(target);
    });

    connect(ui->webEngineView, &QWebEngineView::urlChanged, this, [&](const QUrl &url)
    {
        ui->addressLineEdit->setText(url.toString());
        ++navigationGeneration;
        scriptPending = false;
        if (credentialFillTimer) credentialFillTimer->stop();
        updateCredentialControls();
    });

    connect(ui->webEngineView, &QWebEngineView::loadStarted, this, [this]()
    {
        ++navigationGeneration;
        scriptPending = false;
        if (credentialFillTimer) credentialFillTimer->stop();
    });
    connect(ui->webEngineView, &QWebEngineView::loadFinished, this, [this](bool ok)
    {
        updateCredentialControls();
        if (ok) startCredentialFill();
    });

    connect(ui->webEngineView->page(), &QWebEnginePage::navigationRequested, this,
            [&](QWebEngineNavigationRequest &request) {
                if (request.navigationType() == QWebEngineNavigationRequest::NavigationType::RedirectNavigation &&
                    isCallbackUrl(request.url()))
                {
                    if (!loginCompletedEmitted)
                    {
                        loginCompletedEmitted = true;
                        loginCompleted(request.url().toString());
                    }
                    request.reject();
                    this->close();
                }
                else
                    request.accept();
            });
}

void SsoLoginWebView::setCredentialStore(std::shared_ptr<CredentialStore> store,
                                      const QString &file, const QUrl &server)
{
    credentialStore = std::move(store);
    configurationFile = file;
    credentialVpnServerUrl = server;
    updateCredentialControls();
}

void SsoLoginWebView::setupCredentialControls()
{
    credentialControls = new QWidget(this);
    auto *row = new QHBoxLayout(credentialControls);
    row->setContentsMargins(0, 0, 0, 0);
    saveCredentialsButton = new QPushButton("保存账号密码", credentialControls);
    saveCredentialsButton->setObjectName("saveSsoCredentialsButton");
    fillCredentialsButton = new QPushButton("填充已保存账号", credentialControls);
    fillCredentialsButton->setObjectName("fillSsoCredentialsButton");
    forgetCredentialsButton = new QPushButton("删除本页已保存账号", credentialControls);
    forgetCredentialsButton->setObjectName("forgetSsoCredentialsButton");
    for (auto *button : {saveCredentialsButton, fillCredentialsButton, forgetCredentialsButton})
        button->setAutoDefault(false);
    credentialStatus = new QLabel(credentialControls);
    credentialStatus->setWordWrap(true);
    row->addWidget(saveCredentialsButton);
    row->addWidget(fillCredentialsButton);
    row->addWidget(forgetCredentialsButton);
    row->addWidget(credentialStatus, 1);
    qobject_cast<QVBoxLayout *>(layout())->insertWidget(1, credentialControls);
    credentialControls->hide();
    credentialFillTimer = new QTimer(this);
    credentialFillTimer->setInterval(500);
    connect(credentialFillTimer, &QTimer::timeout, this, &SsoLoginWebView::tryCredentialFill);
    connect(saveCredentialsButton, &QPushButton::clicked, this, &SsoLoginWebView::savePageCredentials);
    connect(fillCredentialsButton, &QPushButton::clicked, this, &SsoLoginWebView::startCredentialFill);
    connect(forgetCredentialsButton, &QPushButton::clicked, this, &SsoLoginWebView::forgetPageCredentials);
}

QString SsoLoginWebView::credentialScope() const
{
    return SsoCredentialScope::key(configurationFile, credentialVpnServerUrl, currentUrl());
}

void SsoLoginWebView::updateCredentialControls()
{
    if (!credentialControls) return;
    credentialControls->setVisible(bool(credentialStore));
    const bool enabled = credentialStore && !credentialScope().isEmpty();
    saveCredentialsButton->setEnabled(enabled);
    fillCredentialsButton->setEnabled(enabled);
    forgetCredentialsButton->setEnabled(enabled);
    credentialStatus->setText(enabled ? "在网页输入账号密码后，点保存；下次自动填充。"
                                      : "仅支持 HTTPS 登录页的账号保存与填充。");
}

QString SsoLoginWebView::credentialScript(const QString &operation, const QUrl &url,
                                        const QString &username, const QString &password) const
{
    QFile file(":/resource/sso-credentials.js");
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QJsonObject request{{"operation", operation},
                              {"origin", SsoCredentialScope::httpsOrigin(url)},
                              {"username", username}, {"password", password}};
    return QString::fromUtf8(file.readAll()) + "(" +
        QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact)) + ")";
}

void SsoLoginWebView::startCredentialFill()
{
    if (!credentialStore || credentialScope().isEmpty()) return;
    fillAttempts = 0;
    credentialFillTimer->start();
    tryCredentialFill();
}

void SsoLoginWebView::tryCredentialFill()
{
    if (scriptPending) return;
    if (!credentialStore || credentialScope().isEmpty() || ++fillAttempts > 30)
    {
        credentialFillTimer->stop();
        return;
    }
    const QUrl pageUrl = currentUrl();
    const QString scope = credentialScope();
    const int generation = navigationGeneration;
    QPointer<SsoLoginWebView> guard(this);
    scriptPending = true;
    ui->webEngineView->page()->runJavaScript(credentialScript("inspect", pageUrl),
        [guard, pageUrl, scope, generation](const QVariant &value)
        {
            if (!guard || guard->navigationGeneration != generation) return;
            guard->scriptPending = false;
            if (value.toMap().value("status").toString() != "ready") return;
            guard->credentialFillTimer->stop();
            const auto store = guard->credentialStore;
            const auto loaded = store->load(scope);
            // Secure-store access can open a native dialog and process navigation events.
            if (!guard || guard->navigationGeneration != generation) return;
            if (loaded.status == CredentialStoreStatus::NotFound) return;
            if (loaded.status != CredentialStoreStatus::Found)
            {
                guard->credentialStatus->setText("无法读取已保存账号；请检查系统凭据库，可手动登录或点击填充重试。");
                return;
            }
            guard->ui->webEngineView->page()->runJavaScript(
                guard->credentialScript("fill", pageUrl, loaded.credentials.username, loaded.credentials.password),
                [guard, generation](const QVariant &filled)
                {
                    if (!guard || guard->navigationGeneration != generation) return;
                    const QString status = filled.toMap().value("status").toString();
                    if (status == "filled") guard->credentialStatus->setText("已填充保存的账号，请在网页确认登录。");
                    else if (status == "different-account") guard->credentialStatus->setText("网页已有其他账号；清空后可点击填充。");
                    else if (status == "already-filled") guard->credentialStatus->setText("网页已有密码，保留当前输入。");
                });
        });
}

void SsoLoginWebView::savePageCredentials()
{
    if (!credentialStore || credentialScope().isEmpty()) return;
    credentialFillTimer->stop();
    const QString scope = credentialScope();
    const int generation = navigationGeneration;
    QPointer<SsoLoginWebView> guard(this);
    saveCredentialsButton->setEnabled(false);
    ui->webEngineView->page()->runJavaScript(credentialScript("capture", currentUrl()),
        [guard, scope, generation](const QVariant &value)
        {
            if (!guard || guard->navigationGeneration != generation) return;
            guard->saveCredentialsButton->setEnabled(true);
            const auto captured = value.toMap();
            if (captured.value("status").toString() != "captured")
            {
                guard->credentialStatus->setText("请在网页的账号登录框输入账号和密码后，再点保存。");
                return;
            }
            const auto store = guard->credentialStore;
            const auto result = store->save(scope,
                {captured.value("username").toString(), captured.value("password").toString()});
            if (!guard || guard->navigationGeneration != generation) return;
            guard->credentialStatus->setText(result.status == CredentialStoreStatus::Success
                ? "已保存到系统凭据库，下次打开本页自动填充。"
                : "未能确认保存成功；请检查系统凭据库及访问权限后重试。");
        });
}

void SsoLoginWebView::forgetPageCredentials()
{
    if (!credentialStore || credentialScope().isEmpty()) return;
    credentialFillTimer->stop();
    ++navigationGeneration; // Invalidate any pending fill or capture callback.
    scriptPending = false;
    const int generation = navigationGeneration;
    QPointer<SsoLoginWebView> guard(this);
    const auto store = credentialStore;
    const auto result = store->remove(credentialScope());
    if (!guard || guard->navigationGeneration != generation) return;
    credentialStatus->setText(result.status == CredentialStoreStatus::Success ||
                             result.status == CredentialStoreStatus::NotFound
        ? "本页已保存账号已删除。网页中已填的内容仍可手动清空。"
        : "未能确认删除成功，请检查系统凭据库及访问权限后重试。");
}

void SsoLoginWebView::setInitialUrl(const QUrl &url)
{
    if (!url.isEmpty())
    {
        ui->addressLineEdit->setText(url.toString());
        ui->webEngineView->load(url);
    }
}

void SsoLoginWebView::setCallbackServerUrl(const QUrl &url)
{
    callbackServerUrl = url;
}

bool SsoLoginWebView::isCallbackUrl(const QUrl &url) const
{
    const auto effectivePort = [](const QUrl &value)
    {
        const int defaultPort =
            value.scheme().compare("https", Qt::CaseInsensitive) == 0 ? 443 : 80;
        return value.port(defaultPort);
    };
    return url.scheme().compare(
               callbackServerUrl.scheme(),
               Qt::CaseInsensitive
           ) == 0
        && url.host().compare(
               callbackServerUrl.host(),
               Qt::CaseInsensitive
           ) == 0
        && effectivePort(url) == effectivePort(callbackServerUrl);
}

QUrl SsoLoginWebView::currentUrl() const
{
    return ui->webEngineView->url();
}

void SsoLoginWebView::closeEvent(QCloseEvent *event)
{
    ++navigationGeneration;
    if (credentialFillTimer) credentialFillTimer->stop();
    scriptPending = false;
    if (!loginCompletedEmitted)
    {
        loginCompletedEmitted = true;
        loginCompleted(QString());
    }

    QDialog::closeEvent(event);
}
