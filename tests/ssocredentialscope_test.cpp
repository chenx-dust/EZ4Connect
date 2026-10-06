#include <QCoreApplication>
#include <QDebug>

#include "application/ssocredentialscope.h"

namespace
{
bool check(bool condition, const char *description)
{
    if (!condition)
    {
        qCritical() << description;
    }
    return condition;
}

bool rejectsUnsafeUrls()
{
    bool passed = true;
    for (const QString &value : {
             QStringLiteral("http://sso.example/login"),
             QStringLiteral("https://user@sso.example/login"),
             QStringLiteral("https://user:password@sso.example/login"),
             QStringLiteral("https://@sso.example/login"),
             QStringLiteral("https://sso.example:0/login"),
             QStringLiteral("https://sso.example:65536/login"),
             QStringLiteral("file:///tmp/login.html"),
             QStringLiteral("/login"),
             QStringLiteral("https://")
         })
    {
        passed &= check(
            SsoCredentialScope::httpsOrigin(QUrl(value)).isEmpty(),
            "Unsafe URL was accepted as a credential origin"
        );
    }
    return passed;
}

bool normalizesAndIsolatesOrigins()
{
    const QString origin = SsoCredentialScope::httpsOrigin(
        QUrl(QStringLiteral("https://SSO.example:443/login?ticket=test#fragment"))
    );
    bool passed = check(origin == QStringLiteral("https://sso.example"), "HTTPS origin did not normalize");
    passed &= check(
        SsoCredentialScope::httpsOrigin(QUrl(QStringLiteral("https://sso.example:8443/login")))
            == QStringLiteral("https://sso.example:8443"),
        "A nondefault port was lost"
    );
    passed &= check(
        SsoCredentialScope::httpsOrigin(QUrl(QStringLiteral("https://sso.example.evil.test/login"))) != origin,
        "A deceptive suffix matched a trusted origin"
    );
    passed &= check(
        SsoCredentialScope::httpsOrigin(QUrl(QStringLiteral("https://[::1]:443/login")))
            == QStringLiteral("https://[::1]"),
        "IPv6 HTTPS origin did not normalize"
    );
    return passed;
}

bool scopesConfigurationAndVpn()
{
    const QUrl vpn(QStringLiteral("https://vpn.example:443"));
    const QUrl page(QStringLiteral("https://sso.example/login"));
    const QString scope = SsoCredentialScope::key(QStringLiteral("/tmp/test/config.ini"), vpn, page);
    bool passed = check(!scope.isEmpty(), "A valid credential scope was empty");
    passed &= check(
        scope == SsoCredentialScope::key(
            QStringLiteral("/tmp/test/../test/config.ini"),
            QUrl(QStringLiteral("https://VPN.example/path?one=1")),
            QUrl(QStringLiteral("https://sso.example:443/other?nonce=new#fragment"))
        ),
        "Transient paths, queries, or equivalent ports changed the scope"
    );
    passed &= check(
        scope != SsoCredentialScope::key(QStringLiteral("/tmp/test/other.ini"), vpn, page),
        "Different configurations shared a credential scope"
    );
    passed &= check(
        scope != SsoCredentialScope::key(
            QStringLiteral("/tmp/test/config.ini"),
            QUrl(QStringLiteral("https://vpn.other.example")),
            page
        ),
        "Different VPN servers shared a credential scope"
    );
    passed &= check(
        scope != SsoCredentialScope::key(
            QStringLiteral("/tmp/test/config.ini"),
            vpn,
            QUrl(QStringLiteral("https://sso.example:8443/login"))
        ),
        "Different SSO ports shared a credential scope"
    );
    passed &= check(SsoCredentialScope::key({}, vpn, page).isEmpty(), "Empty configuration was accepted");
    passed &= check(
        SsoCredentialScope::key(QStringLiteral("/tmp/config.ini"), vpn, QUrl(QStringLiteral("http://sso.example"))).isEmpty(),
        "An unsafe SSO URL was accepted"
    );
    passed &= check(
        SsoCredentialScope::key(QStringLiteral("/tmp/config.ini"), QUrl(QStringLiteral("http://vpn.example")), page).isEmpty(),
        "An unsafe VPN URL was accepted"
    );
    return passed;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const bool passed = rejectsUnsafeUrls() & normalizesAndIsolatesOrigins() & scopesConfigurationAndVpn();
    return passed ? 0 : 1;
}
