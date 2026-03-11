#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSysInfo>
#include <QNetworkInterface>
#include <QClipboard>
#include <QDesktopServices>
#include <QFileInfo>
#include <QCoreApplication>
#include <QActionGroup>
#include <QInputDialog>

#include "mainwindow.h"

#include "ui_mainwindow.h"
#include "utils/utils.h"
#include "zjuconnectcontroller/zjuconnectcontroller.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    zjuConnectController = nullptr;
    profileManager = nullptr;
    const QString overrideConfigPath = Utils::getArgValue(QCoreApplication::arguments(), "--config-path");
    if (overrideConfigPath.isEmpty())
    {
        profileManager = new ProfileManager();
        currentProfileId = profileManager->activeProfile();
        settings = new QSettings(profileManager->profilePath(currentProfileId), QSettings::IniFormat);
    }
    else
    {
        currentProfileId = "custom";
        settings = new QSettings(overrideConfigPath, QSettings::IniFormat);
    }
    diagnosisContext = nullptr;
    newProfileAction = nullptr;
    renameProfileAction = nullptr;
    deleteProfileAction = nullptr;

    upgradeSettings();

    isFirstTimeSetMode = true;
    isZjuConnectLinked = false;
    isSystemProxySet = false;
    zjuConnectError = ZJU_ERROR::NONE;

    ui->setupUi(this);
    setupProfileMenu();

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
                settingWindow = new SettingWindow(this, settings, currentProfileId);
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

                Utils::clearClientData(currentProfileId);
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
    if (isZjuConnectLinked)
    {
        event->ignore();
        hide();
        showNotification("EZ4Connect", "程序已最小化到系统托盘，单击图标可恢复窗口。", QSystemTrayIcon::MessageIcon::Information);
    }
    else
    {
        event->accept();
    }
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
        "当前配置：" + (currentProfileId.isEmpty() ? "默认" : currentProfileId) + "\n"
        "配置路径：" + settings->fileName() + "\n");
}

void MainWindow::setupProfileMenu()
{
    if (profileManager == nullptr)
    {
        ui->profileMenu->setEnabled(false);
        return;
    }

    newProfileAction = ui->profileMenu->addAction("新建配置");
    renameProfileAction = ui->profileMenu->addAction("重命名当前配置");
    deleteProfileAction = ui->profileMenu->addAction("删除当前配置");
    ui->profileMenu->addSeparator();

    connect(newProfileAction, &QAction::triggered, this, &MainWindow::createProfile);
    connect(renameProfileAction, &QAction::triggered, this, &MainWindow::renameCurrentProfile);
    connect(deleteProfileAction, &QAction::triggered, this, &MainWindow::deleteCurrentProfile);

    refreshProfileMenu();
}

void MainWindow::refreshProfileMenu()
{
    if (profileManager == nullptr)
    {
        return;
    }

    const QList<QAction *> actions = ui->profileMenu->actions();
    bool remove = false;
    for (QAction *action : actions)
    {
        if (action == newProfileAction || action == renameProfileAction || action == deleteProfileAction)
        {
            continue;
        }
        if (action->isSeparator())
        {
            remove = true;
            continue;
        }
        if (remove)
        {
            ui->profileMenu->removeAction(action);
            delete action;
        }
    }

    QActionGroup *switchGroup = new QActionGroup(ui->profileMenu);

    switchGroup->setExclusive(true);
    QAction *action = ui->profileMenu->addAction("默认");
    action->setCheckable(true);
    action->setChecked(currentProfileId.isEmpty());
    switchGroup->addAction(action);
    connect(action, &QAction::triggered, this, [this]()
    {
        switchProfile("");
    });

    for (const QString &profileId : profileManager->listProfiles())
    {
        QAction *action = ui->profileMenu->addAction(profileId);
        action->setCheckable(true);
        action->setChecked(profileId == currentProfileId);
        switchGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, profileId]()
        {
            switchProfile(profileId);
        });
    }

    renameProfileAction->setEnabled(!currentProfileId.isEmpty());
    deleteProfileAction->setEnabled(!currentProfileId.isEmpty());
}

bool MainWindow::switchProfile(const QString &profileId)
{
    if (profileManager == nullptr)
    {
        return false;
    }

    if (!QFileInfo::exists(profileManager->profilePath(profileId)))
    {
        refreshProfileMenu();
        return false;
    }

    if (profileId == currentProfileId)
    {
        return true;
    }

    if (isZjuConnectLinked)
    {
        QMessageBox::warning(this, "切换失败", "请先断开 VPN 连接，再切换配置。");
        refreshProfileMenu();
        return false;
    }

    if (settingWindow != nullptr)
    {
        settingWindow->close();
    }

    settings->sync();
    delete settings;
    settings = new QSettings(profileManager->profilePath(profileId), QSettings::IniFormat);
    currentProfileId = profileId;
    profileManager->setActiveProfile(currentProfileId);

    updateVersionInfo();
    upgradeSettings();
    initZjuConnect();
    refreshProfileMenu();

    addLog("已切换到配置：" + currentProfileId);
    return true;
}

void MainWindow::createProfile()
{
    if (profileManager == nullptr)
    {
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(this, "新建配置", "请输入配置名称：\n（仅支持字母、数字、下划线）", QLineEdit::Normal, "", &ok);
    if (!ok)
    {
        return;
    }

    const QString newProfileId = profileManager->createProfile(name, settings->fileName());
    if (newProfileId.isEmpty())
    {
        QMessageBox::critical(this, "创建失败", "无法创建新配置。");
        return;
    }

    switchProfile(newProfileId);
}

void MainWindow::renameCurrentProfile()
{
    if (profileManager == nullptr)
    {
        return;
    }

    bool ok = false;
    QString name = QInputDialog::getText(this, "重命名配置", "请输入新配置名称：\n（仅支持字母、数字、下划线）", QLineEdit::Normal, currentProfileId, &ok);
    if (!ok)
    {
        return;
    }

    const QString normalizedName = profileManager->normalizeProfileId(name);
    if (normalizedName.isEmpty())
    {
        QMessageBox::warning(this, "重命名失败", "配置名称不能为空。");
        return;
    }
    if (normalizedName == currentProfileId)
    {
        return;
    }
    if (isZjuConnectLinked)
    {
        QMessageBox::warning(this, "重命名失败", "请先断开 VPN 连接，再重命名配置。");
        return;
    }

    settings->sync();
    if (settingWindow != nullptr)
    {
        settingWindow->close();
    }
    if (!profileManager->renameProfile(currentProfileId, normalizedName))
    {
        QMessageBox::warning(this, "重命名失败", "目标配置已存在，或当前配置不可重命名。");
        return;
    }

    currentProfileId = normalizedName;
    profileManager->setActiveProfile(currentProfileId);
    delete settings;
    settings = new QSettings(profileManager->profilePath(currentProfileId), QSettings::IniFormat);
    updateVersionInfo();
    refreshProfileMenu();
    clearLog();
    addLog("当前配置已重命名为：" + currentProfileId);
}

void MainWindow::deleteCurrentProfile()
{
    if (profileManager == nullptr)
    {
        return;
    }

    if (currentProfileId.isEmpty())
    {
        QMessageBox::warning(this, "删除失败", "默认配置不可删除。");
        return;
    }

    QMessageBox messageBox(this);
    messageBox.setWindowTitle("删除配置");
    messageBox.setText("确认删除当前配置 \"" + currentProfileId + "\" 吗？");
    messageBox.addButton(QMessageBox::Yes)->setText("是");
    messageBox.addButton(QMessageBox::No)->setText("否");
    messageBox.setDefaultButton(QMessageBox::No);
    if (messageBox.exec() != QMessageBox::Yes)
    {
        return;
    }

    const QString removedProfileId = currentProfileId;
    if (!switchProfile(""))
    {
        return;
    }

    if (!profileManager->removeProfile(removedProfileId))
    {
        QMessageBox::warning(this, "删除失败", "无法删除该配置文件。");
        return;
    }

    refreshProfileMenu();
    addLog("已删除配置：" + removedProfileId);
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
    else if (configVersion == 6)
    {
        profileManager->setAutoStartEnabled(settings->value("Common/AutoStart", false).toBool());
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
        "当前配置：" + (currentProfileId.isEmpty() ? "默认" : currentProfileId)
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
    if (settings != nullptr)
    {
        settings->sync();
        delete settings;
    }

    if (profileManager != nullptr)
    {
        delete profileManager;
    }

    if (zjuConnectController != nullptr)
    {
        disconnect(zjuConnectController, &ZjuConnectController::finished, nullptr, nullptr);
        delete zjuConnectController;
    }

    delete ui;
}
