#ifndef UTILS_H
#define UTILS_H

#include <QString>
#include <QByteArray>
#include <QNetworkReply>
#include <QWidget>
#include <QSettings>

namespace Utils
{
    const inline QString REPO_NAME("chenx-dust/EZ4Connect");
    const inline QString CORE_REPO_NAME("Mythologyli/zju-connect");

    const inline QString APP_NAME("EZ4Connect");

    constexpr inline int CONFIG_VERSION = 5;

    QString consoleOutputToQString(const QByteArray &byteArray);

    void setWidgetFixedWhenHidden(QWidget *widget);

    void showAboutMessageBox(QWidget *parent = nullptr);

    bool isSystemProxySet();

    void setSystemProxy(int http_port, int socks_port, const QString &bypass);

    void clearSystemProxy();

    QString getIpv4Address(const QString &interfaceName);

    void setAutoStart(bool enable);

    bool credentialCheck(const QString &username, const QString &password);

	void resetDefaultSettings(QSettings &settings);

    QString getCorePath();

    QString checkCoreVersion(QObject *parent);

    bool isRunningAsAdmin();

    // Windows only.
    bool relaunchAsAdmin(const QStringList &extraArgs = {});

    bool promptForSudoPassword(QString &password, QWidget *parent = nullptr);

    void clearClientData();
    
    QString getClientDataPath();

    QString getLogFilePath();

    QString getConfigPath();

    QString getArgValue(const QStringList &args, const QString &key);
}

#endif //UTILS_H
