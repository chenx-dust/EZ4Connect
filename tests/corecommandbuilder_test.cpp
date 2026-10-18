#include <QCoreApplication>
#include <QDebug>

#include "infrastructure/coreprocess/corecommandbuilder.h"

namespace
{
bool expectEqual(const QStringList &actual, const QStringList &expected, const char *testName)
{
    if (actual == expected)
    {
        return true;
    }

    qCritical().noquote() << testName << "failed"
                          << "\nexpected:" << expected.join('|')
                          << "\nactual:  " << actual.join('|');
    return false;
}

bool expectEnvironmentValue(
    const CoreCommand &command,
    const QString &name,
    const QString &expected,
    const char *testName
)
{
    const QString actual = command.environmentVariables.value(name);
    if (actual == expected)
    {
        return true;
    }

    qCritical().noquote() << testName << "failed for" << name;
    return false;
}

bool buildsMinimalCommand()
{
    ConnectionProfile profile;
    profile.endpoint.protocol = "easyconnect";

    const CoreCommand command = CoreCommandBuilder::build(profile);
    return expectEqual(command.arguments, {"-protocol", "easyconnect"}, "buildsMinimalCommand")
        && expectEqual(command.loggableArguments, command.arguments, "minimalCommandIsLoggable")
        && command.environmentVariables.isEmpty();
}

bool addsGraphCaptchaFileForEasyConnect()
{
    ConnectionProfile profile;
    profile.endpoint = {
        "easyconnect",
        "cas",
        "domain",
        "86-123",
        "vpn.example.edu",
        443
    };
    profile.behavior.updateBestNodesInterval = 30;

    CoreRuntimePaths runtimePaths;
    runtimePaths.graphCodeFile = "/tmp/easyconnect-graph.jpg";
    runtimePaths.clientDataFile = "/tmp/atrust-client-data.json";
    const CoreCommand command = CoreCommandBuilder::build(profile, runtimePaths);
    return expectEqual(
        command.arguments,
        {
            "-protocol", "easyconnect",
            "-graph-code-file", "/tmp/easyconnect-graph.jpg",
            "-server", "vpn.example.edu",
            "-port", "443"
        },
        "addsGraphCaptchaFileForEasyConnect"
    );
}

bool buildsCompleteCommandInCompatibleOrder()
{
    ConnectionProfile profile;
    profile.endpoint = {"atrust", "cas", "domain", "86-123", "vpn.example.edu", 8443};
    profile.credentials = {"alice", "secret", "TOTP", "/tmp/client.p12", "cert-secret"};
    profile.dns = {"10.0.0.1", false, "10.0.0.2", 60, true,
                   "example.org=1.1.1.1", "223.5.5.5:53", "127.0.0.1:5353"};
    profile.proxy = {"127.0.0.1:1080", "127.0.0.1:1081", "ss://url", "http://direct",
                     true, "example.org"};
    profile.tunnel = {true, true, true, true, true, "127.0.0.1:80/10.0.0.1:80",
                      "127.0.0.1:53/10.0.0.1:53"};
    profile.behavior = {30, true, KeepAliveMode::Url, "https://keepalive", "en0", true, true,
                        true, true};
    profile.debug = {true, true, true};
    profile.extraArguments = "-foo bar";

    const CoreRuntimePaths runtimePaths{
        "/tmp/graph.jpg",
        "/tmp/client-data.json",
        "/tmp/debug capture.pcap",
        "/tmp/debug capture.keys.log"
    };
    const CoreCommand command = CoreCommandBuilder::build(profile, runtimePaths);
    const QStringList expected{
        "-protocol", "atrust",
        "-graph-code-file", "/tmp/graph.jpg",
        "-auth-type", "auth/cas",
        "-client-data-file", "/tmp/client-data.json",
        "-login-domain", "domain",
        "-phone", "86-123",
        "-update-best-nodes-interval", "30",
        "-server", "vpn.example.edu",
        "-port", "8443",
        "-remote-dns-server", "10.0.0.1",
        "-dns-ttl", "60",
        "-secondary-dns-server", "10.0.0.2",
        "-local-dns-server", "223.5.5.5:53",
        "-dns-server-bind", "127.0.0.1:5353",
        "-keep-alive-url", "https://keepalive",
        "-bind-interface", "en0",
        "-auto-detect-interface",
        "-disable-remote-dns",
        "-disable-server-config",
        "-proxy-all",
        "-tun-mode",
        "-dns-hijack",
        "-fake-ip",
        "-add-route",
        "-tcp-tunnel-mode",
        "-debug-dump",
        "-debug-pcap-file", "/tmp/debug capture.pcap",
        "-debug-tls-log-file", "/tmp/debug capture.keys.log",
        "-socks-bind", "127.0.0.1:1080",
        "-http-bind", "127.0.0.1:1081",
        "-shadowsocks-url", "ss://url",
        "-dial-direct-proxy", "http://direct",
        "-tcp-port-forwarding", "127.0.0.1:80/10.0.0.1:80",
        "-udp-port-forwarding", "127.0.0.1:53/10.0.0.1:53",
        "-custom-dns", "example.org=1.1.1.1",
        "-foo", "bar"
    };
    return expectEqual(command.arguments, expected, "buildsCompleteCommandInCompatibleOrder")
        && expectEnvironmentValue(command, "ZJU_CONNECT_USERNAME", "alice", "mapsUsernameToEnvironment")
        && expectEnvironmentValue(command, "ZJU_CONNECT_PASSWORD", "secret", "mapsPasswordToEnvironment")
        && expectEnvironmentValue(command, "ZJU_CONNECT_TOTP_SECRET", "TOTP", "mapsTotpToEnvironment")
        && !command.environmentVariables.contains("ZJU_CONNECT_CERT_FILE")
        && !command.environmentVariables.contains("ZJU_CONNECT_CERT_PASSWORD");
}

bool keepsEasyConnectOnlyOptionsOutOfATrustCommand()
{
    ConnectionProfile profile;
    profile.endpoint.protocol = "atrust";
    profile.credentials.certFile = "/tmp/client.p12";
    profile.credentials.certPassword = "cert-secret";
    profile.proxy.customDomains = "example.org";
    profile.behavior.disableMultiLine = true;
    profile.behavior.disableZjuConfig = true;
    profile.behavior.skipDomainResource = true;

    const CoreCommand command = CoreCommandBuilder::build(profile);
    return expectEqual(
        command.arguments,
        {"-protocol", "atrust"},
        "keepsEasyConnectOnlyOptionsOutOfATrustCommand"
    );
}

bool addsEasyConnectOnlyOptionsForEasyConnect()
{
    ConnectionProfile profile;
    profile.endpoint.protocol = "easyconnect";
    profile.credentials.username = "alice";
    profile.credentials.certFile = "/tmp/client.p12";
    profile.credentials.certPassword = "cert-secret";
    profile.proxy.customDomains = "example.org";
    profile.behavior.disableMultiLine = true;
    profile.behavior.disableZjuConfig = true;
    profile.behavior.skipDomainResource = true;

    const CoreCommand command = CoreCommandBuilder::build(profile);
    return expectEqual(
        command.arguments,
        {
            "-protocol", "easyconnect",
            "-disable-multi-line",
            "-disable-zju-config",
            "-skip-domain-resource",
            "-custom-proxy-domain", "example.org"
        },
        "addsEasyConnectOnlyOptionsForEasyConnect"
    )
        && expectEnvironmentValue(command, "ZJU_CONNECT_USERNAME", "alice", "mapsEasyConnectUsernameToEnvironment")
        && expectEnvironmentValue(command, "ZJU_CONNECT_CERT_FILE", "/tmp/client.p12", "mapsCertFileToEnvironment")
        && expectEnvironmentValue(command, "ZJU_CONNECT_CERT_PASSWORD", "cert-secret", "mapsCertPasswordToEnvironment");
}

bool keepsATrustOnlyOptionsOutOfEasyConnectCommand()
{
    ConnectionProfile profile;
    profile.endpoint = {"easyconnect", "cas", "domain", "86-123", QString(), 0};
    profile.behavior.updateBestNodesInterval = 30;

    const CoreRuntimePaths runtimePaths{QString(), "/tmp/structured-client-data.json"};
    const CoreCommand command = CoreCommandBuilder::build(profile, runtimePaths);
    return expectEqual(
        command.arguments,
        {"-protocol", "easyconnect"},
        "keepsATrustOnlyOptionsOutOfEasyConnectCommand"
    );
}

bool excludesCredentialsFromLoggableArguments()
{
    ConnectionProfile profile;
    profile.endpoint.protocol = "easyconnect";
    profile.credentials = {"alice", "secret", "TOTP", "/tmp/client.p12", "cert-secret"};

    const CoreCommand command = CoreCommandBuilder::build(profile);
    const QString logLine = command.loggableArguments.join(' ');
    const bool safe = !logLine.contains("alice")
        && !logLine.contains("secret")
        && !logLine.contains("TOTP")
        && !logLine.contains("/tmp/client.p12");
    if (!safe)
    {
        qCritical() << "excludesCredentialsFromLoggableArguments failed:" << logLine;
    }
    return safe;
}

bool passesCredentialsAsArgumentsWhenEnabled()
{
    ConnectionProfile profile;
    profile.endpoint.protocol = "easyconnect";
    profile.credentials = {
        "alice",
        "secret",
        "TOTP",
        "/tmp/client.p12",
        "cert-secret",
        true
    };

    const CoreCommand command = CoreCommandBuilder::build(profile);
    return expectEqual(
        command.arguments,
        {
            "-cert-file", "/tmp/client.p12",
            "-cert-password", "cert-secret",
            "-username", "alice",
            "-password", "secret",
            "-totp-secret", "TOTP",
            "-protocol", "easyconnect"
        },
        "passesCredentialsAsArgumentsWhenEnabled"
    )
        && expectEqual(
            command.loggableArguments,
            {"-protocol", "easyconnect"},
            "argumentCredentialsStayOutOfLogs"
        )
        && command.environmentVariables.isEmpty();
}

bool clearsManagedVariablesEvenWhenCredentialsAreEmpty()
{
    ConnectionProfile profile;
    profile.endpoint.protocol = "easyconnect";

    const CoreCommand command = CoreCommandBuilder::build(profile);
    return expectEqual(
        command.clearedEnvironmentVariables,
        {
            "ZJU_CONNECT_USERNAME",
            "ZJU_CONNECT_PASSWORD",
            "ZJU_CONNECT_TOTP_SECRET",
            "ZJU_CONNECT_CERT_FILE",
            "ZJU_CONNECT_CERT_PASSWORD"
        },
        "clearsManagedVariablesEvenWhenCredentialsAreEmpty"
    );
}

bool quotesLoggableArgumentsWithoutChangingArguments()
{
    CoreCommand command;
    command.loggableArguments = {
        "-graph-code-file",
        "/tmp/captcha images/graph.jpg",
        "",
        "embedded\"quote",
        "line\nbreak"
    };

    const QStringList originalArguments = command.loggableArguments;
    const QString expected =
        "-graph-code-file \"/tmp/captcha images/graph.jpg\" \"\" "
        "\"embedded\\\"quote\" \"line\\nbreak\"";
    const QString actual = command.loggableCommandLine();
    if (actual != expected || command.loggableArguments != originalArguments)
    {
        qCritical().noquote() << "quotesLoggableArgumentsWithoutChangingArguments failed"
                              << "\nexpected:" << expected
                              << "\nactual:  " << actual;
        return false;
    }
    return true;
}

bool keepsAutomaticKeepAliveTargetOutOfCommand()
{
    ConnectionProfile profile;
    profile.endpoint.protocol = "atrust";
    profile.behavior.keepAliveMode = KeepAliveMode::Automatic;
    profile.behavior.keepAliveUrl = "http://keepalive.example/";

    const CoreCommand command = CoreCommandBuilder::build(profile);
    return expectEqual(
        command.arguments,
        {"-protocol", "atrust"},
        "keepsAutomaticKeepAliveTargetOutOfCommand"
    );
}

bool passesDisableKeepAliveWhenTurnedOff()
{
    ConnectionProfile profile;
    profile.endpoint.protocol = "atrust";
    profile.behavior.keepAliveMode = KeepAliveMode::Off;
    profile.behavior.keepAliveUrl = "http://keepalive.example/";

    const CoreCommand command = CoreCommandBuilder::build(profile);
    return expectEqual(
        command.arguments,
        {"-protocol", "atrust", "-disable-keep-alive"},
        "passesDisableKeepAliveWhenTurnedOff"
    );
}

bool omitsKeepAliveTargetWhenUrlModeHasNoTarget()
{
    ConnectionProfile profile;
    profile.endpoint.protocol = "atrust";
    profile.behavior.keepAliveMode = KeepAliveMode::Url;

    const CoreCommand command = CoreCommandBuilder::build(profile);
    return expectEqual(
        command.arguments,
        {"-protocol", "atrust"},
        "omitsKeepAliveTargetWhenUrlModeHasNoTarget"
    );
}

bool omitsDisabledDebugArtifacts()
{
    ConnectionProfile profile;
    profile.endpoint.protocol = "easyconnect";
    profile.debug.capturePcap = true;
    profile.debug.exportTlsKeys = true;

    const CoreCommand command = CoreCommandBuilder::build(profile);
    return expectEqual(
        command.arguments,
        {"-protocol", "easyconnect"},
        "omitsDisabledDebugArtifacts"
    );
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const bool passed = buildsMinimalCommand()
        && addsGraphCaptchaFileForEasyConnect()
        && buildsCompleteCommandInCompatibleOrder()
        && keepsEasyConnectOnlyOptionsOutOfATrustCommand()
        && addsEasyConnectOnlyOptionsForEasyConnect()
        && keepsATrustOnlyOptionsOutOfEasyConnectCommand()
        && excludesCredentialsFromLoggableArguments()
        && passesCredentialsAsArgumentsWhenEnabled()
        && clearsManagedVariablesEvenWhenCredentialsAreEmpty()
        && quotesLoggableArgumentsWithoutChangingArguments()
        && keepsAutomaticKeepAliveTargetOutOfCommand()
        && passesDisableKeepAliveWhenTurnedOff()
        && omitsKeepAliveTargetWhenUrlModeHasNoTarget()
        && omitsDisabledDebugArtifacts();
    return passed ? 0 : 1;
}
