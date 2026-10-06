#include "authdialogcoordinator.h"

#include <QCheckBox>
#include <QDebug>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWidget>

#include "presentation/dialogs/graphcaptchawindow/graphcaptchawindow.h"
#include "presentation/dialogs/loginwindow/loginwindow.h"
#include "presentation/dialogs/ssologinwebview/ssologinwebview.h"
#include "presentation/dialogs/sudowindow/sudowindow.h"

AuthDialogCoordinator::AuthDialogCoordinator(
    QWidget *parentWidget,
    QSettings *settings,
    QObject *parent
)
    : QObject(parent),
      parentWidget(parentWidget),
      settings(settings)
{
}

void AuthDialogCoordinator::setSettings(QSettings *newSettings)
{
    settings = newSettings;
}

void AuthDialogCoordinator::setCredentialStore(std::shared_ptr<CredentialStore> store)
{
    credentialStore = std::move(store);
}

void AuthDialogCoordinator::requestLogin(
    const QString &username,
    const QString &password
)
{
    if (loginWindow != nullptr)
    {
        loginWindow->raise();
        loginWindow->activateWindow();
        return;
    }

    loginWindow = new LoginWindow(parentWidget);
    loginWindow->setAttribute(Qt::WA_DeleteOnClose);
    loginWindow->setDetail(username, password);
    connect(loginWindow, &LoginWindow::login, this,
            &AuthDialogCoordinator::loginSubmitted);
    loginWindow->show();
}

void AuthDialogCoordinator::requestPhoneNumber(
    const QString &countryCode,
    const QString &phoneNumber
)
{
    if (phoneNumberDialog != nullptr)
    {
        phoneNumberDialog->raise();
        phoneNumberDialog->activateWindow();
        return;
    }

    auto *dialog = new QDialog(parentWidget);
    phoneNumberDialog = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowModality(Qt::WindowModal);
    dialog->setWindowTitle("短信验证");

    auto *layout = new QVBoxLayout(dialog);
    layout->addWidget(new QLabel("请输入接收验证码的手机号码：", dialog));

    auto *phoneLayout = new QHBoxLayout;
    phoneLayout->addWidget(new QLabel("+", dialog));
    auto *countryCodeEdit = new QLineEdit(countryCode, dialog);
    countryCodeEdit->setObjectName("countryCodeLineEdit");
    countryCodeEdit->setMaximumWidth(60);
    countryCodeEdit->setPlaceholderText("国家码");
    phoneLayout->addWidget(countryCodeEdit);
    phoneLayout->addWidget(new QLabel("-", dialog));
    auto *phoneNumberEdit = new QLineEdit(phoneNumber, dialog);
    phoneNumberEdit->setObjectName("phoneNumberLineEdit");
    phoneNumberEdit->setPlaceholderText("手机号码");
    phoneLayout->addWidget(phoneNumberEdit);
    layout->addLayout(phoneLayout);

    auto *saveCheckBox = new QCheckBox("记住手机号码（可在设置中删除）", dialog);
    layout->addWidget(saveCheckBox);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        dialog
    );
    connect(
        buttonBox,
        &QDialogButtonBox::accepted,
        dialog,
        [this, dialog, countryCodeEdit, phoneNumberEdit, saveCheckBox]()
        {
            const QString submittedCountryCode = countryCodeEdit->text().trimmed();
            const QString submittedPhoneNumber = phoneNumberEdit->text().trimmed();
            if (submittedCountryCode.isEmpty() || submittedPhoneNumber.isEmpty())
            {
                QMessageBox::warning(
                    dialog,
                    "警告",
                    "国家码或手机号码不能为空。"
                );
                return;
            }

            emit phoneNumberSubmitted(
                submittedCountryCode,
                submittedPhoneNumber,
                saveCheckBox->isChecked()
            );
            dialog->accept();
        }
    );
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (phoneNumber.isEmpty())
    {
        phoneNumberEdit->setFocus();
    }
    else
    {
        countryCodeEdit->setFocus();
    }
    dialog->show();
}

void AuthDialogCoordinator::requestSudoPassword()
{
    if (sudoWindow != nullptr)
    {
        sudoWindow->raise();
        sudoWindow->activateWindow();
        return;
    }

    sudoWindow = new SudoWindow(parentWidget);
    sudoWindow->setAttribute(Qt::WA_DeleteOnClose);
    connect(sudoWindow, &SudoWindow::sudo, this,
            &AuthDialogCoordinator::sudoPasswordSubmitted);
    connect(sudoWindow, &QDialog::rejected, this,
            [this]()
            {
                emit sudoPasswordSubmitted({}, false);
            });
    sudoWindow->show();
}

void AuthDialogCoordinator::requestGraphCaptcha(const QString &graphFile)
{
    qInfo().noquote() << "需要图形验证码";
    const bool textInputMode = settings == nullptr
        || settings->value("ZJUConnect/Protocol", "easyconnect").toString() == "easyconnect";
    if (graphCaptchaWindow != nullptr)
    {
        graphCaptchaWindow->setGraph(graphFile, textInputMode);
        graphCaptchaWindow->raise();
        graphCaptchaWindow->activateWindow();
        return;
    }

    graphCaptchaWindow = new GraphCaptchaWindow(parentWidget);
    graphCaptchaWindow->setAttribute(Qt::WA_DeleteOnClose);
    graphCaptchaWindow->setGraph(graphFile, textInputMode);
    connect(graphCaptchaWindow, &GraphCaptchaWindow::finishCaptcha, this,
            [this](const QByteArray &captcha)
            {
                qInfo().noquote() << "图形验证码已提交";
                emit interactiveInputSubmitted(captcha + "\n");
            });
    connect(graphCaptchaWindow, &GraphCaptchaWindow::cancelled, this,
            [this]()
            {
                qInfo().noquote() << "图形验证码输入已取消";
                emit interactiveInputCancelled();
            });
    graphCaptchaWindow->show();
}

void AuthDialogCoordinator::requestSmsCode(bool showSkipSecondaryAuthOption)
{
    qInfo().noquote() << "需要短信验证码";

    QDialog dialog(parentWidget);
    dialog.setWindowTitle("短信验证码");

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("请输入短信验证码：", &dialog));

    auto *codeEdit = new QLineEdit(&dialog);
    layout->addWidget(codeEdit);

    auto *skipCheckBox = new QCheckBox("跳过以后的短信验证", &dialog);
    skipCheckBox->setVisible(showSkipSecondaryAuthOption);
    layout->addWidget(skipCheckBox);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog
    );
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    const bool accepted = dialog.exec() == QDialog::Accepted;
    if (!accepted)
    {
        qInfo().noquote() << "短信验证码输入已取消";
        emit interactiveInputCancelled();
        return;
    }

    QByteArray input;
    input = codeEdit->text().toLocal8Bit();
    if (showSkipSecondaryAuthOption && skipCheckBox->isChecked())
    {
        input.prepend('$');
    }

    qInfo().noquote() << "短信验证码已提交";
    emit interactiveInputSubmitted(input + "\n");
}

void AuthDialogCoordinator::requestRadiusCode(bool showSkipSecondaryAuthOption)
{
    qInfo().noquote() << "需要 RADIUS 动态口令（短信验证码）";

    QDialog dialog(parentWidget);
    dialog.setWindowTitle("RADIUS 动态口令");

    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel("请输入收到的短信验证码（RADIUS token）：", &dialog));

    auto *codeEdit = new QLineEdit(&dialog);
    layout->addWidget(codeEdit);

    auto *skipCheckBox = new QCheckBox("跳过以后的二次认证", &dialog);
    skipCheckBox->setVisible(showSkipSecondaryAuthOption);
    layout->addWidget(skipCheckBox);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        &dialog
    );
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    const bool accepted = dialog.exec() == QDialog::Accepted;
    if (!accepted)
    {
        qInfo().noquote() << "RADIUS 动态口令输入已取消";
        emit interactiveInputCancelled();
        return;
    }

    QByteArray input;
    input = codeEdit->text().toLocal8Bit();
    if (showSkipSecondaryAuthOption && skipCheckBox->isChecked())
    {
        input.prepend('$');
    }

    qInfo().noquote() << "RADIUS 动态口令已提交";
    emit interactiveInputSubmitted(input + "\n");
}

void AuthDialogCoordinator::requestTotpCode()
{
    qInfo().noquote() << "需要 TOTP 验证码";
    bool accepted = false;
    const QString totp = QInputDialog::getText(
        parentWidget,
        "TOTP 验证码",
        "请输入 TOTP 验证码：",
        QLineEdit::Normal,
        "",
        &accepted
    );
    if (!accepted)
    {
        qInfo().noquote() << "TOTP 验证码输入已取消";
        emit interactiveInputCancelled();
        return;
    }

    qInfo().noquote() << "TOTP 验证码已提交";
    emit interactiveInputSubmitted(totp.toLocal8Bit() + "\n");
}

void AuthDialogCoordinator::requestSsoLogin()
{
    if (settings == nullptr)
    {
        return;
    }
    if (ssoLoginWebView != nullptr)
    {
        ssoLoginWebView->raise();
        ssoLoginWebView->activateWindow();
        return;
    }

    const QString serverHost =
        settings->value("ZJUConnect/ServerAddress", "trust.hitsz.edu.cn").toString();
    const int serverPort = settings->value("ZJUConnect/ServerPort", 443).toInt();
    QUrl serverUrl;
    serverUrl.setScheme("https");
    serverUrl.setHost(serverHost);
    if (serverPort != 443)
    {
        serverUrl.setPort(serverPort);
    }

    QString ssoUrl = settings->value("ZJUConnect/LoginURL").toString();
    if (ssoUrl.isEmpty())
    {
        QUrl defaultSsoUrl = serverUrl;
        defaultSsoUrl.setPath("/passport/v1/public/casLogin");
        QUrlQuery query;
        query.addQueryItem(
            "sfDomain",
            settings->value("ZJUConnect/LoginDomain").toString()
        );
        defaultSsoUrl.setQuery(query);
        ssoUrl = defaultSsoUrl.toString();
    }
    if (ssoUrl.startsWith('/'))
    {
        ssoUrl = serverUrl.resolved(QUrl(ssoUrl)).toString();
    }

    qInfo().noquote() << QStringLiteral("单点登录：") + ssoUrl;
    ssoLoginWebView = new SsoLoginWebView(parentWidget);
    ssoLoginWebView->setAttribute(Qt::WA_DeleteOnClose);
    ssoLoginWebView->setCallbackServerUrl(serverUrl);
    ssoLoginWebView->setCredentialStore(credentialStore, settings->fileName(), serverUrl);
    ssoLoginWebView->setInitialUrl(QUrl::fromUserInput(ssoUrl));
    connect(ssoLoginWebView, &SsoLoginWebView::loginCompleted, this,
            [this](const QString &url)
            {
                emit interactiveInputSubmitted(url.toLocal8Bit() + "\n");
            });
    ssoLoginWebView->show();
}
