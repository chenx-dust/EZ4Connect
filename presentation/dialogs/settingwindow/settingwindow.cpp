#include <QFileInfo>
#include <QFileDialog>
#include <QDesktopServices>
#include <QMessageBox>
#include <QHostAddress>
#include <QStandardPaths>

#include "settingwindow.h"
#include "ui_settingwindow.h"
#include "application/applicationconstants.h"
#include "application/defaultsettings.h"
#include "core/connectionsessionstate.h"
#include "infrastructure/platform/autostart.h"
#include "infrastructure/storage/applicationpaths.h"
#include "presentation/presentationhelpers.h"
#include "infrastructure/settings/profilemanager.h"

SettingWindow::SettingWindow(QWidget *parent, QSettings *inputSettings, const QString &profileId) :
    QDialog(parent),
    ui(new Ui::SettingWindow)
{
    ui->setupUi(this);

    this->settings = inputSettings;
    this->profileId = profileId;

    setWindowModality(Qt::WindowModal);
    setAttribute(Qt::WA_DeleteOnClose);

    loadSettings();

    connect(ui->openLogDirectoryPushButton, &QPushButton::clicked, this, [this]()
    {
        if (!QDesktopServices::openUrl(
                QUrl::fromLocalFile(ApplicationPaths::logDirectory())
            ))
        {
            QMessageBox::warning(this, "日志目录", "无法打开日志目录。");
        }
    });

    connect(ui->portForwardingPushButton, &QPushButton::clicked,
            [&]()
            {
                extraSettingWindow = new ExtraSettingWindow(this);
                extraSettingWindow->setup(tcpPortForwarding, udpPortForwarding, customDNS, customProxyDomain, extraArguments);

                connect(extraSettingWindow, &ExtraSettingWindow::applied, this,
				[&](const QString& tcpForwarding, const QString& udpForwarding, const QString& customDNS_, const QString& customProxyDomain_, const QString& extraArg)
                    {
                        tcpPortForwarding = tcpForwarding;
                        udpPortForwarding = udpForwarding;
						customDNS = customDNS_;
						customProxyDomain = customProxyDomain_;
						extraArguments = extraArg;
                    });

                extraSettingWindow->show();
            });

    connect(ui->buttonBox->button(QDialogButtonBox::Ok), &QPushButton::clicked, [&]() {
        if (shouldCheckCredential() && !PresentationHelpers::confirmCredentials(
                ui->usernameLineEdit->text(),
                ui->passwordLineEdit->text()
            ))
            return;
        if (isAuthSettingChanged())
            ApplicationPaths::clearClientData(this->profileId);
        applySettings();
        accept();
    });

    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, [&]() {
        if (shouldCheckCredential() && !PresentationHelpers::confirmCredentials(
                ui->usernameLineEdit->text(),
                ui->passwordLineEdit->text()
            ))
            return;
        if (isAuthSettingChanged())
            ApplicationPaths::clearClientData(this->profileId);
        applySettings();
        loadSettings();
    });

    connect(ui->resetDefaultPushButton, &QPushButton::clicked,
        [&]()
        {
            int status = QMessageBox::warning(this, "警告", "将会重置所有设置，是否继续？", QMessageBox::Ok, QMessageBox::Cancel);
            if (status == QMessageBox::Ok)
            {
                settings->clear();
				DefaultSettings::reset(*settings);
				settings->sync();
                loadSettings();
            }
        });

    connect(ui->importPushButton, &QPushButton::clicked,
            [&]()
            {
                QString filename = QFileDialog::getOpenFileName(this, "选择配置文件",
                    QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                    "Config Ini(*.ini);;All Files(*.*)");
                if (filename.isEmpty()) {
                    QMessageBox::critical(this, "错误", "未选择配置文件，不会带来任何更改。");
                    return;
                }
                QSettings newSettings(filename, QSettings::IniFormat);
                for (const auto& key : newSettings.allKeys()) {
                    settings->setValue(key, newSettings.value(key));
                }
                settings->sync();
                loadSettings();
            });

    connect(ui->exportPushButton, &QPushButton::clicked,
            [&]()
            {
                QString filename = QFileDialog::getSaveFileName(this, "选择保存位置",
                    QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                    "Config Ini(*.ini);;All Files(*.*)");
                if (filename.isEmpty())
                {
                    QMessageBox::critical(this, "错误", "未选择配置文件保存位置。");
                    return;
                }
                settings->sync();
                if (QFile::exists(filename))
                    QFile::remove(filename);
                QFile::copy(settings->fileName(), filename);
            });

    connect(ui->passwordVisibleCheckBox, &QCheckBox::checkStateChanged,
        [&](Qt::CheckState state)
        {
            ui->passwordLineEdit->setEchoMode(state == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password);
        });

    connect(ui->totpSecretVisibleCheckBox, &QCheckBox::checkStateChanged,
        [&](Qt::CheckState state)
        {
            ui->totpSecretLineEdit->setEchoMode(state == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password);
        });

    connect(ui->certPasswordVisibleCheckBox, &QCheckBox::checkStateChanged,
        [&](Qt::CheckState state)
        {
            ui->certPasswordLineEdit->setEchoMode(state == Qt::Checked ? QLineEdit::Normal : QLineEdit::Password);
        });

    connect(ui->certFileBrowseButton, &QPushButton::clicked,
        [&]()
        {
            QString filename = QFileDialog::getOpenFileName(this, "选择证书文件",
                QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                "P12 Certificate(*.p12 *.pfx);;All Files(*.*)");
            if (!filename.isEmpty())
            {
                ui->certFileLineEdit->setText(filename);
            }
        });

    connect(ui->authSelectPushButton, &QPushButton::clicked, this, [&]() {
        authInfoWindow = new AuthInfoWindow(this);
        connect(authInfoWindow, &AuthInfoWindow::finishAuthInfo, this,
                [&](const QString &authType, const QString &loginDomain, const QString &loginUrl) {
                    if (authType == "auth/cas")
                    {
                        ui->casRadioButton->setChecked(true);
                        ui->loginUrlLineEdit->setText(loginUrl);
                    }
                    else if (authType == "auth/httpsOauth2")
                    {
                        ui->oauth2RadioButton->setChecked(true);
                        ui->loginUrlLineEdit->setText(loginUrl);
                    }
                    else if (authType == "auth/smsCheckCode")
                    {
                        ui->smsCheckCodeRadioButton->setChecked(true);
                    }
                    else
                    {
                        ui->pswRadioButton->setChecked(true);
                    }
                    ui->loginDomainLineEdit->setText(loginDomain);
        });
        authInfoWindow->fetchAuthInfo(ui->serverAddressLineEdit->text(), ui->serverPortSpinBox->value());
        authInfoWindow->show();
    });
}

SettingWindow::~SettingWindow()
{
    delete ui;
}

bool SettingWindow::shouldCheckCredential()
{
    if (ui->atrustRadioButton->isChecked())
        return ui->pswRadioButton->isChecked();
    else
        return !ui->certFileLineEdit->text().isEmpty();
}

void SettingWindow::loadSettings()
{
    ui->configVersionLabel->setText(
        "当前配置文件版本：" + QString::number(settings->value("Common/ConfigVersion").toInt()) +
        "\n程序配置文件版本：" +
        QString::number(ApplicationConstants::ConfigVersion)
    );
    ui->usernameLineEdit->setText(settings->value("Credential/Username").toString());
    ui->passwordLineEdit->setText(
        QByteArray::fromBase64(settings->value("Credential/Password").toString().toUtf8())
    );
    ui->totpSecretLineEdit->setText(settings->value("Credential/TOTPSecret").toString());
    ui->certFileLineEdit->setText(settings->value("Credential/CertFile").toString());
    ui->certPasswordLineEdit->setText(
        QByteArray::fromBase64(settings->value("Credential/CertPassword").toString().toUtf8())
    );
    ui->credentialsAsArgumentsCheckBox->setChecked(
        settings->value("ZJUConnect/CredentialsAsArguments", false).toBool()
    );

    ProfileManager profileManager;
    ui->autoStartCheckBox->setChecked(profileManager.autoStartEnabled());
    ui->silentStartCheckBox->setChecked(profileManager.silentStartEnabled());
    ui->connectAfterStartCheckBox->setChecked(settings->value("Common/ConnectAfterStart").toBool());
    ui->checkUpdateAfterStartCheckBox->setChecked(settings->value("Common/CheckUpdateAfterStart").toBool());
    ui->autoSetProxyCheckBox->setChecked(settings->value("Common/AutoSetProxy").toBool());
    ui->reconnectTimeSpinBox->setValue(settings->value("Common/ReconnectTime").toInt());
    ui->reconnectTimeSpinBox->setToolTip(
        QString("首次重连前的等待秒数，此后每次等待加倍，最多尝试 %1 次。")
            .arg(ReconnectPolicy{}.maxAttempts)
    );
    ui->autoReconnectCheckBox->setChecked(settings->value("Common/AutoReconnect").toBool());
	ui->systemProxyBypassLineEdit->setText(settings->value("Common/SystemProxyBypass").toString());
    ui->suppressProxyOverrideWarningCheckBox->setChecked(settings->value("Common/SuppressProxyOverrideWarning", false).toBool());


    ui->serverAddressLineEdit->setText(settings->value("ZJUConnect/ServerAddress").toString());
    ui->serverPortSpinBox->setValue(settings->value("ZJUConnect/ServerPort").toInt());
    ui->dnsLineEdit->setText(settings->value("ZJUConnect/DNS").toString());
    ui->dnsAutoCheckBox->setChecked(settings->value("ZJUConnect/DNSAuto").toBool());
    ui->secondaryDnsLineEdit->setText(settings->value("ZJUConnect/SecondaryDNS").toString());
    ui->localDnsServerLineEdit->setText(
        settings->value("ZJUConnect/LocalDNSServer", "").toString()
    );
    ui->dnsServerBindLineEdit->setText(
        settings->value("ZJUConnect/DNSServerBind", "").toString()
    );
    ui->dnsTTLSpinBox->setValue(settings->value("ZJUConnect/DNSTTL").toInt());
    ui->socks5PortSpinBox->setValue(settings->value("ZJUConnect/SOCKS5Port").toInt());
    ui->httpPortSpinBox->setValue(settings->value("ZJUConnect/HTTPPort").toInt());
    ui->shadowsocksUrlLineEdit->setText(settings->value("ZJUConnect/ShadowsocksURL").toString());
    ui->dialDirectProxyLineEdit->setText(settings->value("ZJUConnect/DialDirectProxy").toString());
    ui->updateBestNodesIntervalSpinBox->setValue(
        settings->value("ZJUConnect/UpdateBestNodesInterval", 300).toInt());

    if (settings->value("ZJUConnect/Protocol").toString() == "atrust")
        ui->atrustRadioButton->setChecked(true);
    else
        ui->easyconnectRadioButton->setChecked(true);
    ui->loginDomainLineEdit->setText(settings->value("ZJUConnect/LoginDomain").toString());
    auto authType = settings->value("ZJUConnect/AuthType").toString();
    if (authType == "smsCheckCode")
        ui->smsCheckCodeRadioButton->setChecked(true);
    else if (authType == "cas")
        ui->casRadioButton->setChecked(true);
    else if (authType == "httpsOauth2")
        ui->oauth2RadioButton->setChecked(true);
    else
        ui->pswRadioButton->setChecked(true);
    ui->loginUrlLineEdit->setText(settings->value("ZJUConnect/LoginURL").toString());
    ui->countryCodeLineEdit->setText(settings->value("ZJUConnect/PhoneCountryCode").toString());
    ui->phoneNumberLineEdit->setText(settings->value("ZJUConnect/PhoneNumber").toString());

    ui->multiLineCheckBox->setChecked(settings->value("ZJUConnect/MultiLine").toBool());
    ui->keepAliveCheckBox->setChecked(settings->value("ZJUConnect/KeepAlive").toBool());
    ui->keepAliveUrlLineEdit->setText(settings->value("ZJUConnect/KeepAliveURL", "").toString());
    ui->bindInterfaceLineEdit->setText(settings->value("ZJUConnect/BindInterface", "").toString());
    ui->outsideAccessCheckBox->setChecked(settings->value("ZJUConnect/OutsideAccess").toBool());

    ui->skipDomainResourceCheckBox->setChecked(settings->value("ZJUConnect/SkipDomainResource").toBool());
    ui->disableServerConfigCheckBox->setChecked(settings->value("ZJUConnect/DisableServerConfig").toBool());
    ui->proxyAllCheckBox->setChecked(settings->value("ZJUConnect/ProxyAll").toBool());
    
    ui->zjuDefaultCheckBox->setChecked(settings->value("ZJUConnect/ZJUDefault").toBool());
    ui->disableDNSCheckBox->setChecked(settings->value("ZJUConnect/DisableZJUDNS").toBool());
    ui->detailedDebugCheckBox->setChecked(settings->value("ZJUConnect/Debug").toBool());
    ui->debugPcapCheckBox->setChecked(
        settings->value("ZJUConnect/DebugPCAP", false).toBool()
    );
    ui->debugTlsLogCheckBox->setChecked(
        settings->value("ZJUConnect/DebugTLSLog", false).toBool()
    );

    ui->tunCheckBox->setChecked(settings->value("ZJUConnect/TUNMode").toBool());
    ui->routeCheckBox->setChecked(settings->value("ZJUConnect/AddRoute").toBool());
    ui->dnsHijackCheckBox->setChecked(settings->value("ZJUConnect/DNSHijack").toBool());
    ui->fakeIPCheckBox->setChecked(settings->value("ZJUConnect/FakeIP").toBool());
    ui->tcpTunnelModeCheckBox->setChecked(settings->value("ZJUConnect/TCPTunnelMode").toBool());
    ui->autoDetectInterfaceCheckBox->setChecked(settings->value("ZJUConnect/AutoDetectInterface", false).toBool());

    tcpPortForwarding = settings->value("ZJUConnect/TCPPortForwarding").toString();
    udpPortForwarding = settings->value("ZJUConnect/UDPPortForwarding").toString();
	customDNS = settings->value("ZJUConnect/CustomDNS").toString();
	customProxyDomain = settings->value("ZJUConnect/CustomProxyDomain").toString();
    extraArguments = settings->value("ZJUConnect/ExtraArguments").toString();

    ui->routeCheckBox->setEnabled(ui->tunCheckBox->isChecked());
    ui->dnsHijackCheckBox->setEnabled(ui->tunCheckBox->isChecked());
    ui->fakeIPCheckBox->setEnabled(ui->tunCheckBox->isChecked());

	ui->dnsLineEdit->setDisabled(ui->dnsAutoCheckBox->isChecked());
}

void SettingWindow::applySettings()
{
    ProfileManager profileManager;
    bool oldAutoStart = profileManager.autoStartEnabled();
    bool newAutoStart = ui->autoStartCheckBox->isChecked();
    if (oldAutoStart != newAutoStart)
        AutoStart::setEnabled(ui->autoStartCheckBox->isChecked());
    profileManager.setAutoStartEnabled(newAutoStart);
    profileManager.setSilentStartEnabled(ui->silentStartCheckBox->isChecked());

    settings->setValue("Credential/Username", ui->usernameLineEdit->text());
    settings->setValue("Credential/Password", QString(ui->passwordLineEdit->text().toUtf8().toBase64()));
    settings->setValue("Credential/TOTPSecret", ui->totpSecretLineEdit->text());
    settings->setValue("Credential/CertFile", ui->certFileLineEdit->text());
    settings->setValue("Credential/CertPassword", QString(ui->certPasswordLineEdit->text().toUtf8().toBase64()));
    settings->setValue(
        "ZJUConnect/CredentialsAsArguments",
        ui->credentialsAsArgumentsCheckBox->isChecked()
    );

    settings->setValue("Common/ConnectAfterStart", ui->connectAfterStartCheckBox->isChecked());
    settings->setValue("Common/CheckUpdateAfterStart", ui->checkUpdateAfterStartCheckBox->isChecked());
    settings->setValue("Common/AutoSetProxy", ui->autoSetProxyCheckBox->isChecked());
    settings->setValue("Common/ReconnectTime", ui->reconnectTimeSpinBox->value());
    settings->setValue("Common/AutoReconnect", ui->autoReconnectCheckBox->isChecked());
    settings->setValue("Common/SystemProxyBypass", ui->systemProxyBypassLineEdit->text());
    settings->setValue("Common/SuppressProxyOverrideWarning", ui->suppressProxyOverrideWarningCheckBox->isChecked());


    settings->setValue("ZJUConnect/ServerAddress", ui->serverAddressLineEdit->text());
    settings->setValue("ZJUConnect/ServerPort", ui->serverPortSpinBox->value());
    settings->setValue("ZJUConnect/DNS", ui->dnsLineEdit->text());
    settings->setValue("ZJUConnect/DNSAuto", ui->dnsAutoCheckBox->isChecked());
    settings->setValue("ZJUConnect/SecondaryDNS", ui->secondaryDnsLineEdit->text());
    settings->setValue(
        "ZJUConnect/LocalDNSServer",
        ui->localDnsServerLineEdit->text().trimmed()
    );
    settings->setValue(
        "ZJUConnect/DNSServerBind",
        ui->dnsServerBindLineEdit->text().trimmed()
    );
    settings->setValue("ZJUConnect/DNSTTL", ui->dnsTTLSpinBox->value());
    settings->setValue("ZJUConnect/SOCKS5Port", ui->socks5PortSpinBox->value());
    settings->setValue("ZJUConnect/HTTPPort", ui->httpPortSpinBox->value());
    settings->setValue("ZJUConnect/ShadowsocksURL", ui->shadowsocksUrlLineEdit->text());
    settings->setValue("ZJUConnect/DialDirectProxy", ui->dialDirectProxyLineEdit->text());
    settings->setValue("ZJUConnect/UpdateBestNodesInterval", ui->updateBestNodesIntervalSpinBox->value());

    settings->setValue("ZJUConnect/Protocol", ui->atrustRadioButton->isChecked() ? "atrust" : "easyconnect");
    settings->setValue(
        "ZJUConnect/EasyConnectAuthType",
        ui->certFileLineEdit->text().isEmpty() ? "password" : "certificate"
    );
    settings->setValue("ZJUConnect/LoginDomain", ui->loginDomainLineEdit->text());
    QString authType;
    if (ui->smsCheckCodeRadioButton->isChecked())
        authType = "smsCheckCode";
    else if (ui->casRadioButton->isChecked())
        authType = "cas";
    else if (ui->oauth2RadioButton->isChecked())
        authType = "httpsOauth2";
    else
        authType = "psw";
    settings->setValue("ZJUConnect/AuthType", authType);
    settings->setValue("ZJUConnect/LoginURL", ui->loginUrlLineEdit->text());
    settings->setValue("ZJUConnect/PhoneCountryCode", ui->countryCodeLineEdit->text());
    settings->setValue("ZJUConnect/PhoneNumber", ui->phoneNumberLineEdit->text());

    settings->setValue("ZJUConnect/MultiLine", ui->multiLineCheckBox->isChecked());
    settings->setValue("ZJUConnect/KeepAlive", ui->keepAliveCheckBox->isChecked());
    settings->setValue("ZJUConnect/KeepAliveURL", ui->keepAliveUrlLineEdit->text().trimmed());
    settings->setValue("ZJUConnect/BindInterface", ui->bindInterfaceLineEdit->text().trimmed());
    settings->setValue("ZJUConnect/OutsideAccess", ui->outsideAccessCheckBox->isChecked());

    settings->setValue("ZJUConnect/SkipDomainResource", ui->skipDomainResourceCheckBox->isChecked());
    settings->setValue("ZJUConnect/DisableServerConfig", ui->disableServerConfigCheckBox->isChecked());
    settings->setValue("ZJUConnect/ProxyAll", ui->proxyAllCheckBox->isChecked());

    settings->setValue("ZJUConnect/DisableZJUDNS", ui->disableDNSCheckBox->isChecked());
    settings->setValue("ZJUConnect/ZJUDefault", ui->zjuDefaultCheckBox->isChecked());
    settings->setValue("ZJUConnect/Debug", ui->detailedDebugCheckBox->isChecked());
    settings->setValue("ZJUConnect/DebugPCAP", ui->debugPcapCheckBox->isChecked());
    settings->setValue("ZJUConnect/DebugTLSLog", ui->debugTlsLogCheckBox->isChecked());

    settings->setValue("ZJUConnect/TUNMode", ui->tunCheckBox->isChecked());
    settings->setValue("ZJUConnect/AddRoute", ui->routeCheckBox->isChecked());
    settings->setValue("ZJUConnect/DNSHijack", ui->dnsHijackCheckBox->isChecked());
    settings->setValue("ZJUConnect/FakeIP", ui->fakeIPCheckBox->isChecked());
    settings->setValue("ZJUConnect/TCPTunnelMode", ui->tcpTunnelModeCheckBox->isChecked());
    settings->setValue("ZJUConnect/AutoDetectInterface", ui->autoDetectInterfaceCheckBox->isChecked());


    settings->setValue("ZJUConnect/TCPPortForwarding", tcpPortForwarding);
    settings->setValue("ZJUConnect/UDPPortForwarding", udpPortForwarding);
    settings->setValue("ZJUConnect/CustomDNS", customDNS);
    settings->setValue("ZJUConnect/CustomProxyDomain", customProxyDomain);
    settings->setValue("ZJUConnect/ExtraArguments", extraArguments);

    settings->setValue(
        "Common/ConfigVersion",
        ApplicationConstants::ConfigVersion
    );

    settings->sync();
}

bool SettingWindow::isAuthSettingChanged()
{
    if (ui->atrustRadioButton->isChecked() == false &&
        settings->value("ZJUConnect/Protocol").toString() != "atrust")
        return false;
    if (ui->atrustRadioButton->isChecked() == true &&
        settings->value("ZJUConnect/Protocol").toString() != "atrust")
        return true;
    QString currentAuthType;
    if (ui->casRadioButton->isChecked())
        currentAuthType = "cas";
    else if (ui->oauth2RadioButton->isChecked())
        currentAuthType = "httpsOauth2";
    else if (ui->smsCheckCodeRadioButton->isChecked())
        currentAuthType = "smsCheckCode";
    else
        currentAuthType = "psw";

    return currentAuthType != settings->value("ZJUConnect/AuthType").toString() ||
           ui->loginDomainLineEdit->text() != settings->value("ZJUConnect/LoginDomain").toString() ||
           ((currentAuthType == "cas" || currentAuthType == "httpsOauth2") &&
            ui->loginUrlLineEdit->text() != settings->value("ZJUConnect/LoginURL").toString()) ||
           ui->serverAddressLineEdit->text() != settings->value("ZJUConnect/ServerAddress").toString() ||
           ui->serverPortSpinBox->value() != settings->value("ZJUConnect/ServerPort").toInt();
}
