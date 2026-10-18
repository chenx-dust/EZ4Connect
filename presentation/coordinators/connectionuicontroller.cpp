#include "connectionuicontroller.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDebug>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QWidget>

#include <utility>

#include "application/applicationlogger.h"
#include "application/connectionsession.h"
#include "application/systemproxysession.h"
#include "infrastructure/coreprocess/coreexecutable.h"
#include "infrastructure/platform/privileges.h"
#include "infrastructure/settings/settingsprofileloader.h"
#include "presentation/coordinators/authdialogcoordinator.h"

ConnectionUiController::ConnectionUiController(
    QWidget *parentWidget,
    QPushButton *connectButton,
    QPushButton *proxyButton,
    QAction *trayConnectAction,
    ConnectionSession *connectionSession,
    SystemProxySession *systemProxySession,
    AuthDialogCoordinator *authenticationDialogs,
    ApplicationLogger *applicationLogger,
    SettingsProvider settingsProvider,
    ProfileIdProvider profileIdProvider,
    NotificationHandler notificationHandler,
    QObject *parent
)
    : QObject(parent),
      parentWidget(parentWidget),
      connectButton(connectButton),
      proxyButton(proxyButton),
      trayConnectAction(trayConnectAction),
      connectionSession(connectionSession),
      systemProxySession(systemProxySession),
      authenticationDialogs(authenticationDialogs),
      settingsProvider(std::move(settingsProvider)),
      profileIdProvider(std::move(profileIdProvider)),
      notificationHandler(std::move(notificationHandler))
{
    connectButton->setText("连接服务器");
    trayConnectAction->setText("连接服务器");
    proxyButton->setText("设置系统代理");
    proxyButton->hide();

    connect(
        connectionSession,
        &ConnectionSession::outputRead,
        applicationLogger,
        &ApplicationLogger::appendCoreOutput
    );
    connect(
        connectionSession,
        &ConnectionSession::savedSudoPasswordRejected,
        this,
        []() { qWarning().noquote() << "sudo 密码可能有误，不使用记住的密码"; }
    );
    connect(
        authenticationDialogs,
        &AuthDialogCoordinator::loginSubmitted,
        this,
        [this](const QString &username, const QString &password, bool saveDetails)
        {
            if (saveDetails)
            {
                settings()->setValue("Credential/Username", username);
                settings()->setValue(
                    "Credential/Password",
                    QString(password.toUtf8().toBase64())
                );
                settings()->sync();
            }
            startConnection(username, password);
        }
    );
    connect(
        authenticationDialogs,
        &AuthDialogCoordinator::phoneNumberSubmitted,
        this,
        [this](
            const QString &countryCode,
            const QString &phoneNumber,
            bool saveDetails
        )
        {
            if (saveDetails)
            {
                settings()->setValue("ZJUConnect/PhoneCountryCode", countryCode);
                settings()->setValue("ZJUConnect/PhoneNumber", phoneNumber);
                settings()->sync();
            }

            const QString username =
                settings()->value("Credential/Username", "").toString();
            const QString password = QByteArray::fromBase64(
                settings()->value("Credential/Password", "").toString().toUtf8()
            );
            startConnection(
                username,
                password,
                countryCode + "-" + phoneNumber
            );
        }
    );
    connect(
        connectionSession,
        &ConnectionSession::reconnectScheduled,
        this,
        [connectionSession](int delayMs)
        {
            qInfo().noquote()
                << QString("正在尝试重新连接（第 %1/%2 次，%3 毫秒后）...")
                       .arg(connectionSession->reconnectAttempt())
                       .arg(connectionSession->reconnectAttemptLimit())
                       .arg(delayMs);
        }
    );
    connect(
        connectionSession,
        &ConnectionSession::reloginRequired,
        this,
        [this](int code)
        {
            // The core could not bring the session back on its own, so a login is
            // the only way forward: ask for one instead of reconnecting silently.
            qWarning().noquote() << "会话已被服务器结束，需要重新登录";
            QString message = "会话已被服务器结束，需要重新登录。";
            if (code > 0)
            {
                message += "\n核心返回码：code=" + QString::number(code);
            }
            this->notificationHandler(
                "VPN",
                message,
                QSystemTrayIcon::MessageIcon::Warning
            );

            const QString username =
                settings()->value("Credential/Username", "").toString();
            const QString password = QByteArray::fromBase64(
                settings()->value("Credential/Password", "").toString().toUtf8()
            );
            this->authenticationDialogs->requestLogin(username, password);
        }
    );
    connect(
        connectionSession,
        &ConnectionSession::finished,
        this,
        [this](ZJU_ERROR error)
        {
            qInfo().noquote() << "VPN 断开！";
            const ConnectionState state = this->connectionSession->state();
            // A session the portal ended already has its own response - the
            // re-login dialog - so it is neither an unexplained disconnect nor a
            // modal error box on top of that dialog.
            const bool reloginRequired = state == ConnectionState::ReloginRequired;
            if (error != ZJU_ERROR::NONE && !reloginRequired)
            {
                this->notificationHandler(
                    "VPN",
                    "VPN 意外断开！",
                    QSystemTrayIcon::MessageIcon::Warning
                );
            }
            this->connectButton->setText("连接服务器");
            this->trayConnectAction->setText("连接服务器");
            this->proxyButton->hide();
            if (state != ConnectionState::Interrupted && !reloginRequired)
            {
                showConnectionError(error);
            }
        }
    );
    connect(
        connectButton,
        &QPushButton::clicked,
        this,
        &ConnectionUiController::handleConnectClicked
    );
    connect(
        proxyButton,
        &QPushButton::clicked,
        this,
        &ConnectionUiController::handleProxyClicked
    );

}

QSettings *ConnectionUiController::settings() const
{
    return settingsProvider();
}

void ConnectionUiController::handleConnectClicked()
{
    if (connectionSession->isActive())
    {
        connectionSession->stop();
        return;
    }

    if (settings()->contains("ZJUConnect/ServerAddress") &&
        settings()->value("ZJUConnect/ServerAddress").toString().isEmpty())
    {
        QMessageBox::critical(parentWidget, "错误", "服务器地址不能为空");
        return;
    }

    const QString username = settings()->value("Credential/Username", "").toString();
    const QString password = QByteArray::fromBase64(
        settings()->value("Credential/Password", "").toString().toUtf8()
    );
    const QString protocol =
        settings()->value("ZJUConnect/Protocol", "easyconnect").toString();
    const QString authType =
        settings()->value("ZJUConnect/AuthType", "psw").toString();
    const QString easyconnectAuthType = settings()->value(
        "ZJUConnect/EasyConnectAuthType",
        settings()->value("Credential/CertFile", "").toString().isEmpty()
            ? "password"
            : "certificate"
    ).toString();

    if (protocol == "easyconnect"
        && easyconnectAuthType == "certificate"
        && settings()->value("Credential/CertFile", "").toString().isEmpty())
    {
        QMessageBox::information(
            parentWidget,
            "需要配置证书",
            "当前配置选择了证书认证。\n"
            "请通过“文件 → 配置引导”选择证书文件。"
        );
        return;
    }

#if defined(Q_OS_WIN)
    if (settings()->value("ZJUConnect/TUNMode").toBool() &&
        !Privileges::isElevated())
    {
        if (Privileges::relaunchElevated())
        {
            QApplication::quit();
        }
        else
        {
            QMessageBox::warning(
                parentWidget,
                "提升失败",
                "无法以管理员权限重新启动，请手动以管理员方式运行。"
            );
        }
        return;
    }
#endif

    const bool passwordLogin =
        (protocol == "atrust" && authType == "psw") ||
        (protocol == "easyconnect" && easyconnectAuthType != "certificate");
    if (protocol == "atrust" && authType == "smsCheckCode")
    {
        QString countryCode = settings()
            ->value("ZJUConnect/PhoneCountryCode", "86")
            .toString()
            .trimmed();
        const QString phoneNumber = settings()
            ->value("ZJUConnect/PhoneNumber", "")
            .toString()
            .trimmed();
        if (countryCode.isEmpty() || phoneNumber.isEmpty())
        {
            if (countryCode.isEmpty())
            {
                countryCode = "86";
            }
            authenticationDialogs->requestPhoneNumber(countryCode, phoneNumber);
            return;
        }
    }
    if (passwordLogin && (username.isEmpty() || password.isEmpty()))
    {
        authenticationDialogs->requestLogin(username, password);
        return;
    }
    startConnection(username, password);
}

void ConnectionUiController::handleProxyClicked()
{
    if (systemProxySession->isBusy())
    {
        return;
    }
    if (systemProxySession->isEnabled())
    {
        systemProxySession->disable();
        return;
    }

    const int httpPort = settings()->value("ZJUConnect/HTTPPort").toInt();
    const int socksPort = settings()->value("ZJUConnect/SOCKS5Port").toInt();
    const SystemProxyConfig proxyConfig{
        httpPort,
        socksPort,
        settings()->value("Common/SystemProxyBypass").toString()
    };
    connect(
        systemProxySession,
        &SystemProxySession::conflictCheckFinished,
        this,
        [this, proxyConfig, httpPort, socksPort](bool conflict)
        {
            if (!connectionSession->isActive())
            {
                return;
            }

            if (conflict &&
                !settings()->value(
                    "Common/SuppressProxyOverrideWarning",
                    false
                ).toBool())
            {
                QMessageBox messageBox(
                    QMessageBox::Warning,
                    "警告",
                    "当前已存在系统代理配置（可能是 Clash 或其它代理软件）\n"
                    "是否覆盖当前系统代理配置？",
                    QMessageBox::Yes | QMessageBox::No,
                    parentWidget
                );
                auto *dontShowCheckBox = new QCheckBox("不再提示");
                messageBox.setCheckBox(dontShowCheckBox);
                if (messageBox.exec() == QMessageBox::No)
                {
                    return;
                }
                if (dontShowCheckBox->isChecked())
                {
                    settings()->setValue(
                        "Common/SuppressProxyOverrideWarning",
                        true
                    );
                    settings()->sync();
                }
            }
            else if (conflict)
            {
                qInfo().noquote() << "跳过系统代理覆盖警告，因为已设置了不再提示";
            }

            qInfo().noquote()
                << "设置系统代理：HTTP端口 " + QString::number(httpPort)
                       + "，SOCKS5 端口 " + QString::number(socksPort);
            systemProxySession->enable(proxyConfig);
        },
        Qt::SingleShotConnection
    );
    systemProxySession->checkConflict(proxyConfig);
}

void ConnectionUiController::startConnection(
    const QString &username,
    const QString &password,
    const QString &phone
)
{
    ConnectionProfile profile = SettingsProfileLoader::load(
        *settings(),
        profileIdProvider(),
        username,
        password
    );
    if (!phone.isNull())
    {
        profile.endpoint.phone = phone;
    }
    profile.program = CoreExecutable::path();
    const ReconnectPolicy reconnectPolicy{
        settings()->value("Common/AutoReconnect", false).toBool(),
        // The ladder's base: "自动重连（秒）" is the first retry's delay, and every
        // further attempt doubles it up to the policy's own budget.
        settings()->value("Common/ReconnectTime", 1).toInt() * 1000
    };
    if (!connectionSession->start(profile, reconnectPolicy)
        || !connectionSession->isActive())
    {
        return;
    }

    connectButton->setText("断开服务器");
    trayConnectAction->setText("断开服务器");
    proxyButton->show();

    if (settings()->value("Common/AutoSetProxy", false).toBool())
    {
        proxyButton->click();
    }
}

void ConnectionUiController::showConnectionError(ZJU_ERROR error)
{
    QString message;
    switch (error)
    {
    case ZJU_ERROR::INVALID_DETAIL:
        message = "登录失败！\n请检查设置中的网络账号和密码是否设置正确。";
        break;
    case ZJU_ERROR::BRUTE_FORCE:
        message = "登录失败！\n登录尝试过于频繁，IP 被风控，请稍后重试或换用 EasyConnect。";
        break;
    case ZJU_ERROR::OTHER_LOGIN_FAILED:
        message = "登录失败！\n未知原因，可将日志反馈给开发者以便调查。";
        break;
    case ZJU_ERROR::ACCESS_DENIED:
        message = "权限不足！\n请关闭程序，点击右键以管理员身份运行。";
        break;
    case ZJU_ERROR::LISTEN_FAILED:
        message = "监听失败！\n请关闭占用端口的程序（如残留的 zju-connect.exe），或者监听其它端口。";
        break;
    case ZJU_ERROR::CLIENT_FAILED:
        message = "连接失败！\n可能是响应超时，请检查本地网络配置是否正常，服务器设置是否正确。";
        break;
    case ZJU_ERROR::CAPTCHA_FAILED:
        message = "登录失败！\n验证码问题，可能是已验证码过期或者有误。";
        break;
    case ZJU_ERROR::PROGRAM_NOT_FOUND:
        message = "程序未找到！\n请检查核心是否在正确路径下，检查是否解压在当前目录下。";
        break;
    case ZJU_ERROR::INTERACTIVE_ERROR:
        message = "登录失败！\n请检查您的输入是否正确，检查是否完成 SSO 登录。";
        break;
    case ZJU_ERROR::AUTH_NOT_AVAILABLE:
        message = "认证方式/登录域不可用！\n请通过“获取认证方式”按钮配置认证方式。";
        break;
    case ZJU_ERROR::AUTH_EXPIRED:
        message = "认证已过期！\n请重新登录。";
        break;
    case ZJU_ERROR::SESSION_EXPIRED:
        message = "会话已失效！\n服务器已结束当前会话，请在登录窗口中重新登录。";
        break;
    case ZJU_ERROR::OTHER:
        message = "其它错误！\n未知原因，可将日志反馈给开发者以便调查。";
        break;
    case ZJU_ERROR::NONE:
        return;
    }
    QMessageBox::critical(parentWidget, "错误", message);
}
