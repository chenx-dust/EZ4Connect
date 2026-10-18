#ifndef CONNECTIONPROFILE_H
#define CONNECTIONPROFILE_H

#include <QString>

struct ConnectionCredentials
{
    QString username;
    QString password;
    QString totpSecret;
    QString certFile;
    QString certPassword;
    bool passAsArguments = false;
};

struct ConnectionEndpoint
{
    QString protocol;
    QString authType;
    QString loginDomain;
    QString phone;
    QString server;
    int port = 0;
};

struct DnsOptions
{
    QString primary;
    bool automatic = false;
    QString secondary;
    int ttl = 3600;
    bool disableZjuDns = false;
    QString custom;
    QString localDnsServer;
    QString dnsServerBind;
};

struct ProxyOptions
{
    QString socksBind;
    QString httpBind;
    QString shadowsocksUrl;
    QString dialDirectProxy;
    bool proxyAll = false;
    QString customDomains;
};

struct TunnelOptions
{
    bool tunMode = false;
    bool addRoute = false;
    bool dnsHijack = false;
    bool fakeIp = false;
    bool tcpTunnelMode = false;
    QString tcpPortForwarding;
    QString udpPortForwarding;
};

// How the core is told to keep a session alive. Some gateways drop a session
// that sees no traffic, so the default is to let the core keep it busy.
enum class KeepAliveMode
{
    // Leave the target to the core, which picks one the tunnel carries.
    Automatic,
    // Probe the address the user named.
    Url,
    // Pass -disable-keep-alive.
    Off
};

inline QString keepAliveModeToString(KeepAliveMode mode)
{
    switch (mode)
    {
    case KeepAliveMode::Url:
        return QStringLiteral("url");
    case KeepAliveMode::Off:
        return QStringLiteral("off");
    case KeepAliveMode::Automatic:
        break;
    }
    return QStringLiteral("auto");
}

// Unknown values fall back to the default instead of failing, so a settings
// file written by another version still loads.
inline KeepAliveMode keepAliveModeFromString(const QString &value)
{
    const QString mode = value.trimmed().toLower();
    if (mode == QStringLiteral("url"))
    {
        return KeepAliveMode::Url;
    }
    if (mode == QStringLiteral("off"))
    {
        return KeepAliveMode::Off;
    }
    return KeepAliveMode::Automatic;
}

struct ConnectionBehavior
{
    int updateBestNodesInterval = 300;
    bool disableMultiLine = false;
    KeepAliveMode keepAliveMode = KeepAliveMode::Automatic;
    QString keepAliveUrl;
    QString bindInterface;
    bool autoDetectInterface = false;
    bool skipDomainResource = false;
    bool disableServerConfig = false;
    bool disableZjuConfig = false;
};

struct DebugOptions
{
    bool detailedOutput = false;
    bool capturePcap = false;
    bool exportTlsKeys = false;
};

struct ConnectionProfile
{
    QString program;
    QString profileId;
    ConnectionCredentials credentials;
    ConnectionEndpoint endpoint;
    DnsOptions dns;
    ProxyOptions proxy;
    TunnelOptions tunnel;
    ConnectionBehavior behavior;
    DebugOptions debug;
    QString extraArguments;
};

#endif // CONNECTIONPROFILE_H
