#include <QMessageBox>
#include <QInputDialog>
#include <QApplication>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "loginwindow/loginwindow.h"
#include "utils/utils.h"
#include "zjuconnectcontroller/zjuconnectcontroller.h"

void MainWindow::initZjuConnect()
{
    clearLog();

    zjuConnectController = new ZjuConnectController(this);

    disconnect(ui->pushButton1, &QPushButton::clicked, nullptr, nullptr);
    ui->pushButton1->setText("连接服务器");
    disconnect(ui->pushButton2, &QPushButton::clicked, nullptr, nullptr);
    ui->pushButton2->setText("设置系统代理");
    ui->pushButton2->hide();

    // 连接服务器
    connect(zjuConnectController, &ZjuConnectController::outputRead, this,
        [&](const QString &output)
        {
            ui->logPlainTextEdit->appendPlainText(output.trimmed());
        });

    connect(zjuConnectController, &ZjuConnectController::error, this,
        [&](ZJU_ERROR err)
        {
            if (zjuConnectError == ZJU_ERROR::NONE)
            {
                zjuConnectError = err;
            }
        });

    connect(zjuConnectController, &ZjuConnectController::graphCaptcha, this,
        [&](const QString &graphFile) {
            addLog("需要图形验证码");
            graphCaptchaWindow = new GraphCaptchaWindow(this);
            graphCaptchaWindow->setGraph(graphFile);
            graphCaptchaWindow->show();
            connect(graphCaptchaWindow, &GraphCaptchaWindow::finishCaptcha, this, [&](const QByteArray &captcha) {
                addLog("图形验证码用户输入：" + captcha);
                emit WriteToProcess(captcha + "\n");
            });
        });

    connect(zjuConnectController, &ZjuConnectController::smsCode, this, [&]() {
        addLog("需要短信验证码");
        QString smsCode = QInputDialog::getText(this, "短信验证码", "请输入短信验证码：");
        addLog("短信验证码用户输入：" + smsCode);
        emit WriteToProcess(smsCode.toLocal8Bit() + "\n");
    });

    connect(zjuConnectController, &ZjuConnectController::casAuth, this, [&]() {
        ssoLoginWebView = new SsoLoginWebView(this);
        connect(ssoLoginWebView, &SsoLoginWebView::loginCompleted,
                [=](const QString &url) { emit WriteToProcess(url.toLocal8Bit() + "\n"); });

        QString serverHost = settings->value("ZJUConnect/ServerAddress", "trust.hitsz.edu.cn").toString();
        QString ssoUrl = settings->value("ZJUConnect/CasLoginURL").toString();
        if (ssoUrl.isEmpty())
            ssoUrl = "https://" + serverHost +
                     "/passport/v1/public/casLogin?sfDomain=" + settings->value("ZJUConnect/LoginDomain").toString();
        if (ssoUrl.startsWith("/"))
            ssoUrl = "https://" + serverHost + ssoUrl;

        addLog(QStringLiteral("单点登录：") + ssoUrl);
        ssoLoginWebView->setInitialUrl(QUrl::fromUserInput(ssoUrl));
        ssoLoginWebView->setCallbackServerHost(serverHost);
        ssoLoginWebView->show();
    });

    connect(zjuConnectController, &ZjuConnectController::finished, this, [&]()
    {
        if (
            zjuConnectError != ZJU_ERROR::NONE &&
            zjuConnectError != ZJU_ERROR::OTHER &&
            settings->value("Common/AutoReconnect", false).toBool() &&
            isZjuConnectLinked
            )
        {
            QTimer::singleShot(settings->value("Common/ReconnectTime", 1).toInt() * 1000, this, [&]()
            {
                if (isZjuConnectLinked)
                {
                    zjuConnectController->stop();

                    isZjuConnectLinked = false;
                    ui->pushButton1->click();
                }
            });

            return;
        }

        addLog("VPN 断开！");
        showNotification("VPN", "VPN 断开！", QSystemTrayIcon::MessageIcon::Warning);
        isZjuConnectLinked = false;
        ui->pushButton1->setText("连接服务器");
        if (isSystemProxySet)
        {
            ui->pushButton2->click();
        }
        ui->pushButton2->hide();

        switch (zjuConnectError)
        {
        case ZJU_ERROR::INVALID_DETAIL:
            QMessageBox::critical(this, "错误", "登录失败！\n请检查设置中的网络账号和密码是否设置正确。");
            break;
        case ZJU_ERROR::BRUTE_FORCE:
            QMessageBox::critical(this, "错误", "登录失败！\n登录尝试过于频繁，IP 被风控，请稍后重试或换用 EasyConnect。");
            break;
        case ZJU_ERROR::OTHER_LOGIN_FAILED:
            QMessageBox::critical(this, "错误", "登录失败！\n未知原因，可将日志反馈给开发者以便调查。");
            break;
        case ZJU_ERROR::ACCESS_DENIED:
            QMessageBox::critical(this, "错误", "权限不足！\n请关闭程序，点击右键以管理员身份运行。");
            break;
        case ZJU_ERROR::LISTEN_FAILED:
            QMessageBox::critical(this, "错误", "监听失败！\n请关闭占用端口的程序（如残留的 zju-connect.exe），或者监听其它端口。");
            break;
        case ZJU_ERROR::CLIENT_FAILED:
            QMessageBox::critical(this, "错误", "连接失败！\n可能是响应超时，请检查本地网络配置是否正常，服务器设置是否正确。");
            break;
        case ZJU_ERROR::CAPTCHA_FAILED:
            QMessageBox::critical(this, "错误", "登录失败！\n验证码问题，可能是已验证码过期或者有误。");
            break;
        case ZJU_ERROR::PROGRAM_NOT_FOUND:
            QMessageBox::critical(this, "错误", "程序未找到！\n请检查核心是否在正确路径下，检查是否解压在当前目录下。");
            break;
        case ZJU_ERROR::OTHER:
            QMessageBox::critical(this, "错误", "其它错误！\n未知原因，可将日志反馈给开发者以便调查。");
            break;
        case ZJU_ERROR::NONE:
        default:
            break;
        }
        zjuConnectError = ZJU_ERROR::NONE;
    });

    connect(ui->pushButton1, &QPushButton::clicked,
            [&]()
            {
                if (!isZjuConnectLinked)
                {
                    if (settings->contains("ZJUConnect/ServerAddress") &&
                        settings->value("ZJUConnect/ServerAddress").toString().isEmpty())
                    {
                        QMessageBox::critical(this, "错误", "服务器地址不能为空");
                        return;
                    }

                    QString username_ = settings->value("Credential/Username", "").toString();
                    QString password_ = QByteArray::fromBase64(settings->value("Credential/Password", "").toString().toUtf8());
                    QString protocol = settings->value("ZJUConnect/Protocol", "easyconnect").toString();
                    QString authtype = settings->value("ZJUConnect/AuthType", "psw").toString();

                    #if !defined(Q_OS_UNIX)
                    // Linux will elevate only the core process via sudo in the controller.
                    if (settings->value("ZJUConnect/TunMode").toBool() && !Utils::isRunningAsAdmin())
                    {

                        if (Utils::relaunchAsAdmin())
                        {
                            QApplication::quit();
                        }
                        else
                        {
                            QMessageBox::warning(this, "提升失败", "无法以管理员权限重新启动，请手动以管理员方式运行。");
                        }
                        return;
                    }
                    #endif

                    auto startZjuConnect = [this](const QString &username, const QString &password) {
                        QString program_path = Utils::getCorePath();
						QString bind_prefix = settings->value("ZJUConnect/OutsideAccess", false).toBool() ? "[::]:" : "127.0.0.1:";

                        isZjuConnectLinked = true;
                        ui->pushButton1->setText("断开服务器");
                        ui->pushButton2->show();

                        if (settings->value("Common/AutoSetProxy", false).toBool())
                        {
                            ui->pushButton2->click();
                        }

                        QString countryCode = settings->value("ZJUConnect/PhoneCountryCode").toString();
                        QString phoneNumber = settings->value("ZJUConnect/PhoneNumber").toString();
                        QString phone = !countryCode.isEmpty() && !phoneNumber.isEmpty() ? (countryCode + "-" + phoneNumber) : "";

                        zjuConnectController->start(
                            program_path, settings->value("ZJUConnect/Protocol").toString(),
                            settings->value("ZJUConnect/AuthType").toString(),
                            settings->value("ZJUConnect/LoginDomain").toString(), username, password, phone,
                            settings->value("Credential/TOTPSecret").toString(),
                            settings->value("ZJUConnect/ServerAddress").toString(),
                            settings->value("ZJUConnect/ServerPort").toInt(),
                            settings->value("ZJUConnect/DNS").toString(),
                            settings->value("ZJUConnect/DNSAuto").toBool(),
                            settings->value("ZJUConnect/SecondaryDNS").toString(),
                            settings->value("ZJUConnect/DNSTTL").toInt(),
                            bind_prefix + QString::number(settings->value("ZJUConnect/Socks5Port").toInt()),
                            bind_prefix + QString::number(settings->value("ZJUConnect/HttpPort").toInt()),
                            settings->value("ZJUConnect/ShadowsocksURL").toString(),
                            settings->value("ZJUConnect/DialDirectProxy").toString(),
                            !settings->value("ZJUConnect/MultiLine").toBool(),
                            !settings->value("ZJUConnect/KeepAlive").toBool(),
                            settings->value("ZJUConnect/SkipDomainResource").toBool(),
                            settings->value("ZJUConnect/DisableServerConfig").toBool(),
                            settings->value("ZJUConnect/ProxyAll").toBool(),
                            settings->value("ZJUConnect/DisableZJUDNS").toBool(),
                            !settings->value("ZJUConnect/ZJUDefault").toBool(),
                            settings->value("ZJUConnect/Debug").toBool(),
                            settings->value("ZJUConnect/TunMode").toBool(),
                            settings->value("ZJUConnect/AddRoute").toBool(),
                            settings->value("ZJUConnect/DNSHijack").toBool(),
                            settings->value("ZJUConnect/TcpPortForwarding").toString(),
                            settings->value("ZJUConnect/UdpPortForwarding").toString(),
                            settings->value("ZJUConnect/CustomDNS", "").toString(),
                            settings->value("ZJUConnect/CustomProxyDomain", "").toString(),
                            settings->value("Credential/CertFile", "").toString(),
                            QByteArray::fromBase64(settings->value("Credential/CertPassword", "").toString().toUtf8()),
                            settings->value("ZJUConnect/ExtraArguments", "").toString());
                	};

                    if (((protocol == "atrust" && authtype == "psw") ||
                              (protocol == "easyconnect" && settings->value("Credential/CertFile", "").toString().isEmpty())) &&
                             (username_.isEmpty() || password_.isEmpty()))
                    {
                        loginWindow = new LoginWindow(this);
                        loginWindow->setDetail(username_, password_);

                        connect(loginWindow, &LoginWindow::login, this,
                            [&, startZjuConnect](const QString& username, const QString& password, bool saveDetail)
                            {
                                if (saveDetail)
                                {
                                    settings->setValue("Credential/Username", username);
                                    settings->setValue("Credential/Password", QString(password.toUtf8().toBase64()));
                                    settings->sync();
                                }
                                startZjuConnect(username, password);
                            }
                        );
                        loginWindow->show();
                    }
                    else
                    {
                        startZjuConnect(username_, password_);
                    }
                }
                else
                {
                    zjuConnectController->stop();
                }
            });

    // 设置系统代理
    connect(ui->pushButton2, &QPushButton::clicked,
            [&]()
            {
                if (!isSystemProxySet)
                {
                    if (Utils::isSystemProxySet())
                    {
                        int rtn = QMessageBox::warning(this, "警告",
                            "当前已存在系统代理配置（可能是 Clash 或其它代理软件）\n是否覆盖当前系统代理配置？",
                            QMessageBox::Yes | QMessageBox::No);

                        if (rtn == QMessageBox::No)
                        {
                            return;
                        }
                    }

                    Utils::setSystemProxy(settings->value("ZJUConnect/HttpPort").toInt(),
                                          settings->value("ZJUConnect/Socks5Port").toInt(),
                                          settings->value("Common/SystemProxyBypass").toString());
                    ui->pushButton2->setText("清除系统代理");
                    isSystemProxySet = true;
                }
                else
                {
                    Utils::clearSystemProxy();
                    ui->pushButton2->setText("设置系统代理");
                    isSystemProxySet = false;
                    if (!isZjuConnectLinked)
                    {
                        ui->pushButton2->hide();
                    }
                }
            });

    emit SetModeFinished();
}
