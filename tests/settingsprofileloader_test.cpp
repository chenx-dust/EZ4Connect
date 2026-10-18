#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QTemporaryDir>

#include "infrastructure/settings/settingsprofileloader.h"

namespace
{
bool loadsSettingsIntoTypedProfile()
{
    QTemporaryDir directory;
    if (!directory.isValid())
    {
        qCritical() << "Unable to create temporary directory";
        return false;
    }

    QSettings settings(directory.filePath("profile.ini"), QSettings::IniFormat);
    settings.setValue("Credential/TOTPSecret", "totp");
    settings.setValue("Credential/CertFile", "/tmp/client.p12");
    settings.setValue(
        "Credential/CertPassword",
        QString(QStringLiteral("cert-password").toUtf8().toBase64())
    );
    settings.setValue("ZJUConnect/Protocol", "atrust");
    settings.setValue("ZJUConnect/AuthType", "cas");
    settings.setValue("ZJUConnect/LoginDomain", "domain");
    settings.setValue("ZJUConnect/PhoneCountryCode", "86");
    settings.setValue("ZJUConnect/PhoneNumber", "123456");
    settings.setValue("ZJUConnect/ServerAddress", "vpn.example.edu");
    settings.setValue("ZJUConnect/ServerPort", 8443);
    settings.setValue("ZJUConnect/DNS", "10.0.0.1");
    settings.setValue("ZJUConnect/DNSAuto", false);
    settings.setValue("ZJUConnect/SecondaryDNS", "10.0.0.2");
    settings.setValue("ZJUConnect/LocalDNSServer", "223.5.5.5:53");
    settings.setValue("ZJUConnect/DNSServerBind", "127.0.0.1:5353");
    settings.setValue("ZJUConnect/DNSTTL", 60);
    settings.setValue("ZJUConnect/DisableZJUDNS", true);
    settings.setValue("ZJUConnect/CustomDNS", "example.org=1.1.1.1");
    settings.setValue("ZJUConnect/OutsideAccess", true);
    settings.setValue("ZJUConnect/SOCKS5Port", 1080);
    settings.setValue("ZJUConnect/HTTPPort", 1081);
    settings.setValue("ZJUConnect/ShadowsocksURL", "ss://url");
    settings.setValue("ZJUConnect/DialDirectProxy", "http://direct");
    settings.setValue("ZJUConnect/ProxyAll", true);
    settings.setValue("ZJUConnect/CustomProxyDomain", "example.org");
    settings.setValue("ZJUConnect/TUNMode", true);
    settings.setValue("ZJUConnect/AddRoute", true);
    settings.setValue("ZJUConnect/DNSHijack", true);
    settings.setValue("ZJUConnect/FakeIP", true);
    settings.setValue("ZJUConnect/TCPTunnelMode", true);
    settings.setValue("ZJUConnect/TCPPortForwarding", "tcp-forward");
    settings.setValue("ZJUConnect/UDPPortForwarding", "udp-forward");
    settings.setValue("ZJUConnect/UpdateBestNodesInterval", 30);
    settings.setValue("ZJUConnect/CredentialsAsArguments", true);
    settings.setValue("ZJUConnect/MultiLine", false);
    settings.setValue("ZJUConnect/KeepAliveMode", "url");
    settings.setValue("ZJUConnect/KeepAliveURL", "https://keepalive");
    settings.setValue("ZJUConnect/BindInterface", "en0");
    settings.setValue("ZJUConnect/AutoDetectInterface", true);
    settings.setValue("ZJUConnect/SkipDomainResource", true);
    settings.setValue("ZJUConnect/DisableServerConfig", true);
    settings.setValue("ZJUConnect/ZJUDefault", false);
    settings.setValue("ZJUConnect/Debug", true);
    settings.setValue("ZJUConnect/DebugPCAP", true);
    settings.setValue("ZJUConnect/DebugTLSLog", true);
    settings.setValue("ZJUConnect/ExtraArguments", "-foo bar");

    const ConnectionProfile profile =
        SettingsProfileLoader::load(settings, "campus", "alice", "secret");

    const bool passed =
        profile.profileId == "campus"
        && profile.credentials.username == "alice"
        && profile.credentials.password == "secret"
        && profile.credentials.totpSecret == "totp"
        && profile.credentials.certFile.isEmpty()
        && profile.credentials.certPassword.isEmpty()
        && profile.credentials.passAsArguments
        && profile.endpoint.protocol == "atrust"
        && profile.endpoint.authType == "cas"
        && profile.endpoint.loginDomain == "domain"
        && profile.endpoint.phone == "86-123456"
        && profile.endpoint.server == "vpn.example.edu"
        && profile.endpoint.port == 8443
        && profile.dns.primary == "10.0.0.1"
        && !profile.dns.automatic
        && profile.dns.secondary == "10.0.0.2"
        && profile.dns.ttl == 60
        && profile.dns.disableZjuDns
        && profile.dns.custom == "example.org=1.1.1.1"
        && profile.dns.localDnsServer == "223.5.5.5:53"
        && profile.dns.dnsServerBind == "127.0.0.1:5353"
        && profile.proxy.socksBind == "[::]:1080"
        && profile.proxy.httpBind == "[::]:1081"
        && profile.proxy.shadowsocksUrl == "ss://url"
        && profile.proxy.dialDirectProxy == "http://direct"
        && profile.proxy.proxyAll
        && profile.proxy.customDomains == "example.org"
        && profile.tunnel.tunMode
        && profile.tunnel.addRoute
        && profile.tunnel.dnsHijack
        && profile.tunnel.fakeIp
        && profile.tunnel.tcpTunnelMode
        && profile.tunnel.tcpPortForwarding == "tcp-forward"
        && profile.tunnel.udpPortForwarding == "udp-forward"
        && profile.behavior.updateBestNodesInterval == 30
        && profile.behavior.disableMultiLine
        && profile.behavior.keepAliveMode == KeepAliveMode::Url
        && profile.behavior.keepAliveUrl == "https://keepalive"
        && profile.behavior.bindInterface == "en0"
        && profile.behavior.autoDetectInterface
        && profile.behavior.skipDomainResource
        && profile.behavior.disableServerConfig
        && profile.behavior.disableZjuConfig
        && profile.debug.detailedOutput
        && profile.debug.capturePcap
        && profile.debug.exportTlsKeys
        && profile.extraArguments == "-foo bar";

    if (!passed)
    {
        qCritical() << "loadsSettingsIntoTypedProfile failed";
    }
    return passed;
}

bool usesCompatibleDefaults()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("empty.ini"), QSettings::IniFormat);

    const ConnectionProfile profile = SettingsProfileLoader::load(settings, "", "", "");
    const bool passed =
        profile.behavior.updateBestNodesInterval == 300
        && !profile.credentials.passAsArguments
        && profile.behavior.disableMultiLine
        && profile.behavior.keepAliveMode == KeepAliveMode::Automatic
        && profile.behavior.disableZjuConfig
        && profile.dns.localDnsServer.isEmpty()
        && profile.dns.dnsServerBind.isEmpty()
        && !profile.debug.detailedOutput
        && !profile.debug.capturePcap
        && !profile.debug.exportTlsKeys
        && profile.proxy.socksBind == "127.0.0.1:0"
        && profile.proxy.httpBind == "127.0.0.1:0";
    if (!passed)
    {
        qCritical() << "usesCompatibleDefaults failed";
    }
    return passed;
}

bool respectsEasyConnectAuthenticationMode()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("easyconnect.ini"), QSettings::IniFormat);
    settings.setValue("ZJUConnect/Protocol", "easyconnect");
    settings.setValue("Credential/CertFile", "/tmp/client.p12");
    settings.setValue(
        "Credential/CertPassword",
        QString(QStringLiteral("cert-password").toUtf8().toBase64())
    );

    settings.setValue("ZJUConnect/EasyConnectAuthType", "password");
    const ConnectionProfile passwordProfile =
        SettingsProfileLoader::load(settings, "", "alice", "secret");
    if (!passwordProfile.credentials.certFile.isEmpty()
        || !passwordProfile.credentials.certPassword.isEmpty())
    {
        qCritical() << "password mode kept certificate credentials";
        return false;
    }

    settings.setValue("ZJUConnect/EasyConnectAuthType", "certificate");
    const ConnectionProfile certificateProfile =
        SettingsProfileLoader::load(settings, "", "", "");
    const bool passed =
        certificateProfile.credentials.certFile == "/tmp/client.p12"
        && certificateProfile.credentials.certPassword == "cert-password";
    if (!passed)
    {
        qCritical() << "certificate mode did not load certificate credentials";
    }
    return passed;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    return loadsSettingsIntoTypedProfile()
        && usesCompatibleDefaults()
        && respectsEasyConnectAuthenticationMode() ? 0 : 1;
}
