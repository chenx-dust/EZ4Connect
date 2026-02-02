#include "zjuconnectcontroller.h"
#include "mainwindow.h"
#include "utils/utils.h"

ZjuConnectController::ZjuConnectController(QWidget* parent) : QObject(parent)
{
    zjuConnectProcess = new QProcess(this);

    // 初始化日志文件
    logFile = new QFile(Utils::getLogFilePath());
    if (logFile->open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
    {
        logStream = new QTextStream(logFile);
        logStream->setEncoding(QStringConverter::Utf8);
        QString startMsg = "=== Log started at " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") +
                           " with " + QApplication::applicationDisplayName() + " " + QApplication::applicationVersion() +
                           " ===\n";
        *logStream << startMsg;
        logStream->flush();
    }

    auto outputProcess = [&](const QString& output)
        {
            emit outputRead(output);

            // 写入日志文件
            if (logStream != nullptr)
            {
                *logStream << output;
                if (!output.endsWith('\n'))
                {
                    *logStream << '\n';
                }
                logStream->flush();
            }

            if (output.contains("Graph check code saved to "))
            {
                emit graphCaptcha(graphFile);
            }
            else if (output.contains("Please enter the SMS verification code: ") || output.contains("Please enter your SMS code:"))
            {
                emit smsCode();
            }
            else if (output.contains("Please enter the callback url:"))
            {
                emit casAuth();
            }
            else if (output.contains("graph check code still required after second login attempt"))
            {
                emit error(ZJU_ERROR::CAPTCHA_FAILED);
            }
            else if (output.contains("Access is denied."))
            {
                emit error(ZJU_ERROR::ACCESS_DENIED);
            }
            else if (output.contains("listen failed"))
            {
                emit error(ZJU_ERROR::LISTEN_FAILED);
            }
            else if (output.contains("Invalid username or password!") || output.contains("ticket is empty"))
            {
                emit error(ZJU_ERROR::INVALID_DETAIL);
            }
            else if (output.contains("You are trying brute-force login on this IP address."))
            {
                emit error(ZJU_ERROR::BRUTE_FORCE);
            }
            else if (output.contains("Login failed") || output.contains("too many login failures"))
            {
                emit error(ZJU_ERROR::OTHER_LOGIN_FAILED);
            }
            else if (output.contains("unexpected newline"))
            {
                emit error(ZJU_ERROR::INTERACTIVE_ERROR);
            }
            else if (output.contains("auth type/login domain combination not found"))
            {
                emit error(ZJU_ERROR::AUTH_NOT_AVAILABLE);
            }
            else if (output.contains("client setup error"))
            {
                emit error(ZJU_ERROR::CLIENT_FAILED);
            }
            else if (output.contains("panic"))
            {
                emit error(ZJU_ERROR::OTHER);
            }
        };

    connect(zjuConnectProcess, &QProcess::readyReadStandardOutput, this, [&, outputProcess]()
    {
        QString output = Utils::consoleOutputToQString(zjuConnectProcess->readAllStandardOutput());

		outputProcess(output);
    });

    connect(zjuConnectProcess, &QProcess::readyReadStandardError, this, [&, outputProcess]()
    {
        QString output = Utils::consoleOutputToQString(zjuConnectProcess->readAllStandardError());

		outputProcess(output);
    });

    connect(zjuConnectProcess, &QProcess::errorOccurred, this, [&](QProcess::ProcessError err)
    {
        if (stopRequested)
        {
            return;
        }
        QString timeString = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        QString errorString = zjuConnectProcess->errorString();
        emit outputRead(timeString + " 退出原因：" + errorString);

        if (errorString.contains("No such file or directory") || errorString.contains("not found") || errorString.contains("找不到"))
        {
            emit outputRead(timeString + " 核心路径：" + zjuConnectProcess->program());
            emit error(ZJU_ERROR::PROGRAM_NOT_FOUND);
        }
    });

    connect(zjuConnectProcess, &QProcess::finished, this, [&]()
    {
        stopRequested = false;
        QString timeString = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        emit outputRead(timeString + " 退出原因：" "进程已结束");
        emit finished();
    });


    connect(qobject_cast<MainWindow *>(parent), &MainWindow::WriteToProcess, this,
            [&](const QByteArray &data) { zjuConnectProcess->write(data); });
}

void ZjuConnectController::start(
    const QString& program,
    const QString& protocol,
    const QString& authType,
    const QString& loginDomain,
    const QString& username,
    const QString& password,
    const QString& phone,
    const QString& totpSecret,
    const QString& server,
    int port,
    const QString& dns,
    bool dnsAuto,
    const QString& secondaryDns,
    int dnsTtl,
    const QString& socksBind,
    const QString& httpBind,
    const QString& shadowsocksUrl,
    const QString& dialDirectProxy,
    int updateBestNodesInterval,
    bool disableMultiLine,
    bool disableKeepAlive,
    bool skipDomainResource,
    bool disableServerConfig,
    bool proxyAll,
    bool disableZjuDns,
    bool disableZjuConfig,
    bool debugDump,
    bool tunMode,
    bool addRoute,
    bool dnsHijack,
    bool fakeIp,
    bool tcpTunnelMode,
    const QString& tcpPortForwarding,
    const QString& udpPortForwarding,
    const QString& customDNS,
    const QString& customProxyDomain,
    const QString& certFile,
    const QString& certPassword,
    const QString& extraArguments
)
{
    QStringList args;

    if (!protocol.isEmpty())
    {
        args.append("-protocol");
        args.append(protocol);
    }

    if (!authType.isEmpty())
    {
        args.append("-auth-type");
        args.append("auth/" + authType);
    }

    if (protocol == "atrust")
    {
        // 图形验证码文件路径
        if (tempDir == nullptr)
        {
            tempDir = new QTemporaryDir;
            tempDir->setAutoRemove(true);
        }
        graphFile = tempDir->filePath("graph.jpg");
        args.append("-graph-code-file");
        args.append(graphFile);

        // 存放 Client Data
        args.append("-client-data-file");
        args.append(Utils::getClientDataPath());
    }

    if (!phone.isEmpty())
    {
        args.append("-phone");
        args.append(phone);
    }

    if (!loginDomain.isEmpty())
    {
        args.append("-login-domain");
        args.append(loginDomain);
    }

    if (!server.isEmpty())
    {
        args.append("-server");
        args.append(server);
    }

    if (port != 0)
    {
        args.append("-port");
        args.append(QString::number(port));
    }

    if (!dns.isEmpty() || dnsAuto)
    {
        args.append("-zju-dns-server");
		if (dnsAuto)
		{
			args.append("auto");
		}
		else
		{
			args.append(dns);
		}
    }

    if (dnsTtl != 3600)
    {
        args.append("-dns-ttl");
        args.append(QString::number(dnsTtl));
    }

    if (!secondaryDns.isEmpty())
    {
        args.append("-secondary-dns-server");
        args.append(secondaryDns);
    }

    if (disableMultiLine)
    {
        args.append("-disable-multi-line");
    }

    if (disableKeepAlive)
    {
        args.append("-disable-keep-alive");
    }

    if (disableZjuConfig)
    {
        args.append("-disable-zju-config");
    }

    if (disableZjuDns)
    {
        args.append("-disable-zju-dns");
    }

    if (disableServerConfig)
    {
        args.append("-disable-server-config");
    }

    if (proxyAll)
    {
        args.append("-proxy-all");
    }

    if (skipDomainResource)
    {
        args.append("-skip-domain-resource");
    }

    if (tunMode)
    {
        args.append("-tun-mode");

        if (dnsHijack)
        {
            args.append("-dns-hijack");
            if (fakeIp)
            {
                args.append("-fake-ip");
            }
        }

        if (addRoute)
        {
            args.append("-add-route");
        }
    }

    if (tcpTunnelMode)
    {
        args.append("-tcp-tunnel-mode");
    }

    if (debugDump)
    {
        args.append("-debug-dump");
    }

    if (!socksBind.isEmpty())
    {
        args.append("-socks-bind");
        args.append(socksBind);
    }

    if (!httpBind.isEmpty())
    {
        args.append("-http-bind");
        args.append(httpBind);
    }

    if (!shadowsocksUrl.isEmpty())
    {
        args.append("-shadowsocks-url");
        args.append(shadowsocksUrl);
    }

    if (!dialDirectProxy.isEmpty())
    {
        args.append("-dial-direct-proxy");
        args.append(dialDirectProxy);
    }

    if (updateBestNodesInterval != 300)
    {
        args.append("-update-best-nodes-interval");
        args.append(QString::number(updateBestNodesInterval));
    }

    if (!tcpPortForwarding.isEmpty())
    {
        args.append("-tcp-port-forwarding");
        args.append(tcpPortForwarding);
    }

    if (!udpPortForwarding.isEmpty())
    {
        args.append("-udp-port-forwarding");
        args.append(udpPortForwarding);
    }

    if (!customDNS.isEmpty())
    {
        args.append("-custom-dns");
        args.append(customDNS);
    }

    if (!customProxyDomain.isEmpty())
    {
        args.append("-custom-proxy-domain");
        args.append(customProxyDomain);
    }

    if (!extraArguments.isEmpty())
    {
        args.append(extraArguments.split(" "));
    }

    QString timeString = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    emit outputRead(timeString + " VPN 启动！参数：" + args.join(' '));

    QStringList credentialList;

    if (!username.isEmpty())
    {
        credentialList.append("-username");
        credentialList.append(username);
    }

    if (!password.isEmpty())
    {
        credentialList.append("-password");
        credentialList.append(password);
    }

    if (!totpSecret.isEmpty())
    {
        emit outputRead(timeString + " 使用了 TOTP");
        credentialList.append("-totp-secret");
        credentialList.append(totpSecret);
    }

    if (!certFile.isEmpty())
    {
        emit outputRead(timeString + " 使用了证书文件");
        args.append("-cert-file");
        args.append(certFile);
    }

    if (!certPassword.isEmpty())
    {
        args.append("-cert-password");
        args.append(certPassword);
    }

    QString programToStart = program;
    QStringList finalArgs = credentialList + args;

#if defined(Q_OS_UNIX)
    QString sudoPassword;
    bool useSudo = false;
    if (tunMode && !Utils::isRunningAsAdmin())
    {
        if (!Utils::promptForSudoPassword(sudoPassword, qobject_cast<QWidget *>(parent())))
        {
            emit outputRead(timeString + " 已取消 sudo 密码输入");
        }
        else
        {
            QStringList sudoArgs;
            sudoArgs << "-S";
            sudoArgs << programToStart;
            sudoArgs << finalArgs;
            programToStart = "sudo";
            finalArgs = sudoArgs;
            useSudo = true;
            emit outputRead(timeString + " 使用 sudo 提升权限启动核心");
        }
    }
#endif

    zjuConnectProcess->start(programToStart, finalArgs);
#if defined(Q_OS_UNIX)
    if (useSudo)
    {
        zjuConnectProcess->write((sudoPassword + "\n").toUtf8());
        zjuConnectProcess->closeWriteChannel();
    }
#endif
    zjuConnectProcess->waitForStarted();
    if (zjuConnectProcess->state() == QProcess::NotRunning)
    {
        emit finished();
    }
}

void ZjuConnectController::stop()
{
    if (zjuConnectProcess->state() == QProcess::NotRunning)
    {
        return;
    }

    if (!stopRequested)
    {
        stopRequested = true;
        zjuConnectProcess->terminate();
    }
    else
    {
        zjuConnectProcess->kill();
    }
}

ZjuConnectController::~ZjuConnectController()
{
    stop();

    // 关闭日志文件
    if (logStream != nullptr)
    {
        QString endMsg = "=== Log ended at " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " ===\n";
        *logStream << endMsg;
        logStream->flush();
        delete logStream;
        logStream = nullptr;
    }

    if (logFile != nullptr)
    {
        if (logFile->isOpen())
        {
            logFile->close();
        }
        delete logFile;
        logFile = nullptr;
    }
}
