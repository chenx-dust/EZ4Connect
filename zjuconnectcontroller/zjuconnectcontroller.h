#ifndef ZJUCONNECTCONTROLLER_H
#define ZJUCONNECTCONTROLLER_H

#include <QtCore>

enum class ZJU_ERROR
{
    NONE,
    INVALID_DETAIL,
    BRUTE_FORCE,
    OTHER_LOGIN_FAILED,
    ACCESS_DENIED,
    LISTEN_FAILED,
    CLIENT_FAILED,
    CAPTCHA_FAILED,
    PROGRAM_NOT_FOUND,
    OTHER,
};

class ZjuConnectController : public QObject
{
Q_OBJECT

public:
    ZjuConnectController(QWidget* parent);

    ~ZjuConnectController() override;

    void start(
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
        const QString& tcpPortForwarding,
        const QString& udpPortForwarding,
        const QString& customDNS,
        const QString& customProxyDomain,
        const QString& certFile,
        const QString& certPassword,
        const QString& extraArguments
    );

    void stop();


signals:

    void error(ZJU_ERROR err);

    void outputRead(const QString &output);

    void graphCaptcha(const QString &graphFile);

    void smsCode();

    void casAuth();

    void finished();

    void write(const QByteArray &data);

private:
    QProcess *zjuConnectProcess;

    QTemporaryDir *tempDir = nullptr;

    QString graphFile;

    QFile *logFile = nullptr;
    QTextStream *logStream = nullptr;
};

#endif //ZJUCONNECTCONTROLLER_H
