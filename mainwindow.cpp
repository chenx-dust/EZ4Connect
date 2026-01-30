#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <QNetworkInterface>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>

#include "mainwindow.h"

#include "ui_mainwindow.h"
#include "utils/utils.h"
#include "zjuconnectcontroller/zjuconnectcontroller.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    zjuConnectController = nullptr;

    settings = new QSettings(Utils::getConfigPath(), QSettings::IniFormat);
    diagnosisContext = nullptr;

    upgradeSettings();

    isFirstTimeSetMode = true;
    isZjuConnectLinked = false;
    isSystemProxySet = false;
    zjuConnectError = ZJU_ERROR::NONE;

    ui->setupUi(this);

    setWindowIcon(QIcon(QPixmap(":/resource/icon.png").scaled(
        512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation
    )));

    ui->applicationNameLabel->setText(QApplication::applicationDisplayName());

	versionInfo.ui_version = QApplication::applicationVersion();
	versionInfo.ui_latest = "正在检查";
    versionInfo.core_version = "未知";
	versionInfo.core_latest = "正在检查";
    updateVersionInfo();

    // 系统托盘
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setIcon(QIcon(QPixmap(":/resource/icon.png").scaled(
        512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation
    )));
    trayIcon->setVisible(true);
    trayIcon->setToolTip(QApplication::applicationName());
    connect(trayIcon, &QSystemTrayIcon::activated, this, [&](QSystemTrayIcon::ActivationReason reason)
    {
        switch (reason)
        {
            case QSystemTrayIcon::Context:
                trayMenu->exec(QCursor::pos());
                break;
            default:
                show();
                setWindowState(Qt::WindowState::WindowActive);
        }
    });
    trayIcon->show();

    trayShowAction = new QAction("显示", this);
    trayCloseAction = new QAction("退出", this);
    trayMenu = new QMenu(this);
    trayMenu->addAction(trayShowAction);
    trayMenu->addAction(trayCloseAction);
    connect(trayShowAction, &QAction::triggered, this, [&]()
    {
        show();
        setWindowState(Qt::WindowState::WindowActive);
    });
    connect(trayCloseAction, &QAction::triggered, this, [&]()
    {
        QApplication::quit();
    });

    // 文件-退出
    connect(ui->exitAction, &QAction::triggered,
            [&]()
            {
                QApplication::quit();
            });

    // 文件-设置
    connect(ui->settingAction, &QAction::triggered, this,
            [&]()
            {
                settingWindow = new SettingWindow(this, settings);
                settingWindow->show();
            });

    // 文件-打开日志文件
    connect(ui->openLogAction, &QAction::triggered, this,
            [&]()
            {
                QString logFilePath = Utils::getLogFilePath();
                QFileInfo logFileInfo(logFilePath);
                
                if (logFileInfo.exists())
                {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(logFilePath));
                }
                else
                {
                    QMessageBox::information(this, "日志文件", "日志文件还未生成，请先启动 VPN 连接。");
                }
            });

    // 文件-清除系统代理
    connect(ui->disableProxyAction, &QAction::triggered,
            [&]()
            {
                QMessageBox messageBox(this);
                messageBox.setWindowTitle("禁用系统代理");
                messageBox.setText("是否禁用系统代理？");

                messageBox.addButton(QMessageBox::Yes)->setText("是");
                messageBox.addButton(QMessageBox::No)->setText("否");
                messageBox.setDefaultButton(QMessageBox::Yes);

                if (messageBox.exec() == QMessageBox::No)
                {
                    return;
                }

                if (isSystemProxySet)
                {
                    ui->pushButton2->click();
                }
                else
                {
                    Utils::clearSystemProxy();
                }

                addLog("已禁用系统代理设置");
            });

    // 文件-清理登录数据
    connect(ui->clearClientDataAction, &QAction::triggered, this,
            [&]()
            {
                QMessageBox messageBox(this);
                messageBox.setWindowTitle("清理登录缓存");
                messageBox.setText("是否清理登录缓存？");

                messageBox.addButton(QMessageBox::Yes)->setText("是");
                messageBox.addButton(QMessageBox::No)->setText("否");
                messageBox.setDefaultButton(QMessageBox::Yes);

                if (messageBox.exec() == QMessageBox::No)
                {
                    return;
                }

                Utils::clearClientData();
                addLog("已清理登录缓存");
            });

    // 帮助-检查更新
    connect(ui->checkUpdateAction, &QAction::triggered, this,
            [&]()
            {
                checkUpdate();
            });

    // 帮助-项目主页
    connect(ui->projectAction, &QAction::triggered,
            [&]()
            {
                QDesktopServices::openUrl(QUrl("https://github.com/" + Utils::REPO_NAME));
            });

    // 帮助-关于本软件
    connect(ui->aboutAction, &QAction::triggered,
            [&]()
            {
                Utils::showAboutMessageBox(this);
            });

    // 复制日志
    connect(ui->copyLogPushButton, &QPushButton::clicked,
            [&]()
            {
                auto logText = ui->logPlainTextEdit->toPlainText();
                QApplication::clipboard()->setText(logText);
            }
    );

    connect(this, &MainWindow::SetModeFinished, this, 
			[&]()
		    {
		        if (isFirstTimeSetMode)
		        {
                    isFirstTimeSetMode = false;
                    bool shouldConnect = settings->value("Common/ConnectAfterStart", false).toBool();
                    for (const QString &arg : qApp->arguments())
                    {
                        if (arg == "--connect")
                        {
                            shouldConnect = true;
                            break;
                        }
                    }
                    if (shouldConnect)
                    {
		                ui->pushButton1->click();
		            }
		        }
			}
    );

    // 自动检查更新
    checkUpdateNAM = new QNetworkAccessManager(this);

    connect(checkUpdateNAM, &QNetworkAccessManager::finished,
        this, [&](QNetworkReply* reply) {
            if (reply->error() != QNetworkReply::NoError)
            {
                addLog("检查 UI 更新失败。原因是：" + reply->errorString());
                ui->versionLabel->setText(
                    "当前版本：" + QApplication::applicationVersion() + "\n检查 UI 更新失败\n"
                );
                reply->deleteLater();
                return;
            }

            QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
            reply->deleteLater();

            QString nowVersion = QApplication::applicationVersion();
            QString latestVersion = json["tag_name"].toString();

            // 移除开头的 'v'
            if (latestVersion.startsWith('v'))
            {
                latestVersion = latestVersion.mid(1);
            }
            addLog("检查 UI 更新成功。最新版本：" + latestVersion);
			versionInfo.ui_latest = latestVersion;
			updateVersionInfo();

            qsizetype nowVersionSuffix, latestVersionSuffix;
            auto nowVersionQ = QVersionNumber::fromString(nowVersion, &nowVersionSuffix);
            auto latestVersionQ = QVersionNumber::fromString(latestVersion, &latestVersionSuffix);

            if (latestVersionQ > nowVersionQ || 
                (latestVersionQ == nowVersionQ && latestVersion.right(latestVersionSuffix) != nowVersion.right(nowVersionSuffix)))
            {
                QMessageBox msgBox;
                msgBox.setText("UI 版本更新");
                msgBox.setInformativeText("存在 UI 版本更新：" + latestVersion + "\n"
                    "是否前往 Github 发布页面查看？");
                msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
                msgBox.setDefaultButton(QMessageBox::Ok);

                int ret = msgBox.exec();
                if (ret == QMessageBox::Ok)
                {
                    QDesktopServices::openUrl(QUrl("https://github.com/" + Utils::REPO_NAME + "/releases/latest"));
                }
            }
        });

    // 检查核心更新
    checkCoreUpdateNAM = new QNetworkAccessManager(this);

    connect(checkCoreUpdateNAM, &QNetworkAccessManager::finished,
        this, [&](QNetworkReply* reply) {
            if (reply->error() != QNetworkReply::NoError)
            {
                addLog("检查核心更新失败。原因是：" + reply->errorString());
                ui->versionLabel->setText(
                    "当前版本：" + QApplication::applicationVersion() + "\n检查核心更新失败\n"
                );
                reply->deleteLater();
                return;
            }

            QJsonObject json = QJsonDocument::fromJson(reply->readAll()).object();
            reply->deleteLater();

            QString nowVersion = versionInfo.core_version;
            QString latestVersion = json["tag_name"].toString();

            // 移除开头的 'v'
            if (latestVersion.startsWith('v'))
            {
                latestVersion = latestVersion.mid(1);
            }
            addLog("检查核心更新成功。最新版本：" + latestVersion);
            versionInfo.core_latest = latestVersion;
            updateVersionInfo();

            qsizetype nowVersionSuffix, latestVersionSuffix;
            auto nowVersionQ = QVersionNumber::fromString(nowVersion, &nowVersionSuffix);
            auto latestVersionQ = QVersionNumber::fromString(latestVersion, &latestVersionSuffix);

            if (latestVersionQ > nowVersionQ ||
                (latestVersionQ == nowVersionQ && latestVersion.right(latestVersionSuffix) != nowVersion.right(nowVersionSuffix)))
            {
                addLog("核心版本存在更新，可手动更新或通知开发者更新。");
            }
        });

    initZjuConnect();

    if (settings->value("Common/CheckUpdateAfterStart", true).toBool())
    {
        checkUpdate();
    }
    else
    {
		versionInfo.ui_latest = "已禁用";
		versionInfo.core_latest = "已禁用";
		updateVersionInfo();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}

void MainWindow::addLog(const QString &log)
{
    QString timeString = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ui->logPlainTextEdit->appendPlainText(timeString + " " + log.trimmed());
}

void MainWindow::clearLog()
{
    ui->logPlainTextEdit->clear();
    ui->logPlainTextEdit->appendPlainText(
        "欢迎使用 " + QApplication::applicationDisplayName() + "\n"
        "当前版本：" + QApplication::applicationVersion() + "\n"
        "系统版本：" + QSysInfo::prettyProductName() + "\n"
        "配置路径：" + Utils::getConfigPath() + "\n");
}

void MainWindow::checkUpdate()
{
    try
    {
        versionInfo.core_version = Utils::checkCoreVersion(this);
		addLog("检查核心版本成功：" + versionInfo.core_version);
    }
    catch (const std::runtime_error& e)
    {
        addLog("检查核心版本失败：" + QString(e.what()));
        versionInfo.core_version = "错误";
    }
    QNetworkRequest request(QUrl("https://api.github.com/repos/" + Utils::REPO_NAME + "/releases/latest"));
    checkUpdateNAM->get(request);
    QNetworkRequest request_c(QUrl("https://api.github.com/repos/" + Utils::CORE_REPO_NAME + "/releases/latest"));
    checkCoreUpdateNAM->get(request_c);
}

void MainWindow::upgradeSettings()
{
    int configVersion = settings->value("Common/ConfigVersion", -1).toInt();

    if (configVersion == -1)
    {
        Utils::resetDefaultSettings(*settings);
    }
    else if (configVersion == 4)
    {
        settings->setValue("ZJUConnect/Protocol", "easyconnect");
    }
    else if (configVersion < Utils::CONFIG_VERSION)
    {
        QMessageBox msgBox;
        msgBox.setText("存在配置更新");
        msgBox.setInformativeText("建议恢复默认设置，以使用优化的配置。\n\n是否恢复默认设置？");
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Cancel);

        if (msgBox.exec() == QMessageBox::Ok)
        {
            settings->clear();
			Utils::resetDefaultSettings(*settings);
            QMessageBox::information(this, "完成", "已恢复默认设置。");
        }
    }

    settings->setValue("Common/ConfigVersion", Utils::CONFIG_VERSION);
    settings->sync();
}

void MainWindow::updateVersionInfo()
{
	ui->versionLabel->setText(
		"UI 版本：" + versionInfo.ui_version + " 最新：" + versionInfo.ui_latest + "\n"
		"核心版本：" + versionInfo.core_version + " 最新：" + versionInfo.core_latest + "\n"
	);
}

void MainWindow::showNotification(const QString &title, const QString &content, QSystemTrayIcon::MessageIcon icon)
{
    disconnect(trayIcon, &QSystemTrayIcon::messageClicked, nullptr, nullptr);
    trayIcon->showMessage(
        title,
        content,
        icon,
        10000
    );

    connect(trayIcon, &QSystemTrayIcon::messageClicked, this, [&]()
    {
        disconnect(trayIcon, &QSystemTrayIcon::messageClicked, nullptr, nullptr);

        show();
        setWindowState(Qt::WindowState::WindowActive);
    });
}

void MainWindow::cleanUpWhenQuit()
{
    // 保存配置
    if (settings->value("Common/ConfigVersion", 0).toInt() <= Utils::CONFIG_VERSION)
    {
        settings->setValue("Common/ConfigVersion", Utils::CONFIG_VERSION);
    }
    settings->sync();

    // 清除系统代理
    if (isSystemProxySet)
    {
        Utils::clearSystemProxy();
    }
}

MainWindow::~MainWindow()
{
    if (zjuConnectController != nullptr)
    {
        disconnect(zjuConnectController, &ZjuConnectController::finished, nullptr, nullptr);
        delete zjuConnectController;
    }

    delete ui;
}
