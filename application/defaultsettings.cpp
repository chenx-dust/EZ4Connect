#include "application/defaultsettings.h"

#include <QSettings>

#include "application/applicationconstants.h"

void DefaultSettings::reset(QSettings &settings)
{
    settings.setValue("Credential/Username", "");
    settings.setValue("Credential/Password", "");
    settings.setValue("Credential/TOTPSecret", "");

    settings.setValue("Common/ConnectAfterStart", false);
    settings.setValue("Common/CheckUpdateAfterStart", false);
    settings.setValue("Common/AutoSetProxy", false);
    settings.setValue("Common/ReconnectTime", 1);
    settings.setValue("Common/AutoReconnect", false);
    settings.setValue("Common/SystemProxyBypass", "");

    settings.setValue("ZJUConnect/ServerAddress", "trust.hitsz.edu.cn");
    settings.setValue("ZJUConnect/ServerPort", 443);
    settings.setValue("ZJUConnect/DNS", "");
    settings.setValue("ZJUConnect/DNSAuto", true);
    settings.setValue("ZJUConnect/SecondaryDNS", "");
    settings.setValue("ZJUConnect/LocalDNSServer", "");
    settings.setValue("ZJUConnect/DNSServerBind", "");
    settings.setValue("ZJUConnect/DNSTTL", 3600);
    settings.setValue("ZJUConnect/SOCKS5Port", 11080);
    settings.setValue("ZJUConnect/HTTPPort", 11081);
    settings.setValue("ZJUConnect/ShadowsocksURL", "");
    settings.setValue("ZJUConnect/DialDirectProxy", "");
    settings.setValue("ZJUConnect/UpdateBestNodesInterval", 300);
    settings.setValue("ZJUConnect/CredentialsAsArguments", false);

    settings.setValue("ZJUConnect/Protocol", "atrust");
    settings.setValue("ZJUConnect/EasyConnectAuthType", "password");
    settings.setValue("ZJUConnect/LoginDomain", "hitcas");
    settings.setValue("ZJUConnect/AuthType", "cas");
    settings.setValue("ZJUConnect/LoginURL", "");
    settings.setValue("ZJUConnect/PhoneCountryCode", "86");
    settings.setValue("ZJUConnect/PhoneNumber", "");

    settings.setValue("ZJUConnect/MultiLine", false);
    settings.setValue("ZJUConnect/KeepAliveMode", "auto");
    settings.setValue("ZJUConnect/KeepAliveURL", "");
    settings.setValue("ZJUConnect/BindInterface", "");
    settings.setValue("ZJUConnect/OutsideAccess", false);
    settings.setValue("ZJUConnect/SkipDomainResource", false);
    settings.setValue("ZJUConnect/DisableServerConfig", false);
    settings.setValue("ZJUConnect/ProxyAll", false);
    settings.setValue("ZJUConnect/DisableZJUDNS", false);
    settings.setValue("ZJUConnect/ZJUDefault", false);
    settings.setValue("ZJUConnect/Debug", false);
    settings.setValue("ZJUConnect/DebugPCAP", false);
    settings.setValue("ZJUConnect/DebugTLSLog", false);

    settings.setValue("ZJUConnect/TUNMode", false);
    settings.setValue("ZJUConnect/AddRoute", false);
    settings.setValue("ZJUConnect/DNSHijack", false);
    settings.setValue("ZJUConnect/FakeIP", false);
    settings.setValue("ZJUConnect/TCPTunnelMode", false);
    settings.setValue("ZJUConnect/AutoDetectInterface", false);

    settings.setValue("ZJUConnect/TCPPortForwarding", "");
    settings.setValue("ZJUConnect/UDPPortForwarding", "");
    settings.setValue("ZJUConnect/CustomDNS", "");
    settings.setValue("ZJUConnect/CustomProxyDomain", "");
    settings.setValue("ZJUConnect/ExtraArguments", "");

    settings.setValue("Common/ConfigVersion", ApplicationConstants::ConfigVersion);
}
