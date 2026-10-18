#ifndef COREOUTPUTPARSER_H
#define COREOUTPUTPARSER_H

#include <QByteArray>
#include <QString>

enum class CoreOutputEvent
{
    None,
    AskSudoPassword,
    GraphCaptcha,
    SmsCodeWithSkipOption,
    SmsCode,
    TotpCode,
    RandCode,
    RadiusCodeWithSkipOption,
    SsoCallback,
    ClientStarted,
    CaptchaFailed,
    AccessDenied,
    ListenFailed,
    InvalidCredentials,
    BruteForceBlocked,
    LoginFailed,
    InteractiveError,
    AuthNotAvailable,
    AuthExpired,
    SessionRequiresRelogin,
    SessionStateReported,
    KeepAliveReported,
    ClientFailed,
    CorePanic,
};

class CoreOutputParser
{
public:
    static CoreOutputEvent parse(const QString &output);

    static bool hasInteractivePrompt(const QByteArray &output);

    static QString graphCaptchaFile(const QString &output);

    // The portal's code from the "requires reauthentication" line, or 0 when the
    // line carries none.
    static int sessionReloginCode(const QString &output);
};

#endif // COREOUTPUTPARSER_H
