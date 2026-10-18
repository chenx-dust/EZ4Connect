#include "coreoutputparser.h"

#include <QRegularExpression>

CoreOutputEvent CoreOutputParser::parse(const QString &output)
{
    if (output.contains("SUDO_ASK_PASS"))
    {
        return CoreOutputEvent::AskSudoPassword;
    }
    if (output.contains("Graph check code saved to "))
    {
        return CoreOutputEvent::GraphCaptcha;
    }
    if (output.contains("Please enter the SMS verification code: "))
    {
        return CoreOutputEvent::SmsCodeWithSkipOption;
    }
    if (output.contains("Please enter your SMS code:"))
    {
        return CoreOutputEvent::SmsCode;
    }
    if (output.contains("Please enter your TOTP code:"))
    {
        return CoreOutputEvent::TotpCode;
    }
    if (output.contains("Please enter rand code:"))
    {
        return CoreOutputEvent::RandCode;
    }
    // aTrust RADIUS Access-Challenge：核心在密码认证后输出此提示并等待用户输入短信验证码。
    if (output.contains("Please enter the RADIUS token:"))
    {
        return CoreOutputEvent::RadiusCodeWithSkipOption;
    }
    if (output.contains("Please enter the callback url:"))
    {
        return CoreOutputEvent::SsoCallback;
    }
    if (output.contains("VPN client started"))
    {
        return CoreOutputEvent::ClientStarted;
    }
    if (output.contains("graph check code still required after second login attempt") ||
        output.contains("The characters are incorrect or has expired!"))
    {
        return CoreOutputEvent::CaptchaFailed;
    }
    if (output.contains("Access is denied."))
    {
        return CoreOutputEvent::AccessDenied;
    }
    if (output.contains("listen failed"))
    {
        return CoreOutputEvent::ListenFailed;
    }
    if (output.contains("Invalid username or password!") || output.contains("ticket is empty"))
    {
        return CoreOutputEvent::InvalidCredentials;
    }
    if (output.contains("You are trying brute-force login on this IP address."))
    {
        return CoreOutputEvent::BruteForceBlocked;
    }
    if (output.contains("Login failed") || output.contains("too many login failures"))
    {
        return CoreOutputEvent::LoginFailed;
    }
    if (output.contains("unexpected newline"))
    {
        return CoreOutputEvent::InteractiveError;
    }
    if (output.contains("auth type/login domain combination not found"))
    {
        return CoreOutputEvent::AuthNotAvailable;
    }
    if (output.contains("invalid SID") || output.contains("l3-tunnel tunnel auth failed:"))
    {
        return CoreOutputEvent::AuthExpired;
    }
    // The core keeps running when the portal has ended the session, so this line
    // is the ladder's final verdict: "a login is the only way back". The
    // tunnel-layer answers above are not that - the core still tries to recover
    // from them, and a renewal can succeed.
    if (output.contains("aTrust session requires reauthentication"))
    {
        return CoreOutputEvent::SessionRequiresRelogin;
    }
    // What the session maintenance reports about itself, and what the keep-alive
    // reports about each round: both are complete lines a caller is expected to
    // follow, and the details are parsed outside this chain.
    if (output.contains("aTrust session state: "))
    {
        return CoreOutputEvent::SessionStateReported;
    }
    if (output.contains("KeepAlive: "))
    {
        return CoreOutputEvent::KeepAliveReported;
    }
    if (output.contains("client setup error"))
    {
        return CoreOutputEvent::ClientFailed;
    }
    if (output.contains("panic"))
    {
        return CoreOutputEvent::CorePanic;
    }
    return CoreOutputEvent::None;
}

QString CoreOutputParser::graphCaptchaFile(const QString &output)
{
    const QString prefix = "Graph check code saved to ";
    const qsizetype markerIndex = output.indexOf(prefix);
    if (markerIndex < 0)
    {
        return {};
    }

    return output.mid(markerIndex + prefix.size()).trimmed();
}

int CoreOutputParser::sessionReloginCode(const QString &output)
{
    // The line is "aTrust session requires reauthentication (code=75500002)".
    static const QRegularExpression codePattern(QStringLiteral("\\(code=([0-9]+)\\)"));
    const QRegularExpressionMatch match = codePattern.match(output);
    if (!match.hasMatch())
    {
        return 0;
    }
    return match.captured(1).toInt();
}

bool CoreOutputParser::hasInteractivePrompt(const QByteArray &output)
{
    return output.contains("SUDO_ASK_PASS")
           || output.contains("Please enter the SMS verification code: ")
           || output.contains("Please enter your SMS code:")
           || output.contains("Please enter your TOTP code:")
           || output.contains("Please enter rand code:")
           || output.contains("Please enter the RADIUS token:")
           || output.contains("Please enter the callback url:");
}
