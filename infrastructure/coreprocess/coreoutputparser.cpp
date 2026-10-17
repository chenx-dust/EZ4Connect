#include "coreoutputparser.h"

namespace
{

// One rule per literal the core can print. A single table feeds both parse() and
// hasInteractivePrompt(), so an event and its "the core is blocked on stdin"
// flag cannot disagree: that is what silently broke the aTrust prompts, which
// were matched by the event chain but not by the interactive list.
struct OutputRule
{
    const char *literal;
    CoreOutputEvent event;
    bool answerRequired; // the core waits on stdin: flush the line even without a newline
    bool caseInsensitive;
};

// Every literal is the text the core prints. The challenge handler prints
// "<message> " - with a trailing space and no newline - so the prompts below
// arrive as partial lines and only `answerRequired` keeps them from sitting in
// the output buffer forever. Order matters: the first matching rule wins.
constexpr OutputRule kOutputRules[] = {
    {"SUDO_ASK_PASS", CoreOutputEvent::AskSudoPassword, true, false},
    // Graph captcha: the core writes the image to -graph-code-file and asks for
    // the click coordinates as JSON. Older cores report the file they wrote.
    {"Graph check code saved to ", CoreOutputEvent::GraphCaptcha, false, false},
    {"Please enter the graph check code JSON:", CoreOutputEvent::GraphCaptcha, true, false},
    // Second-factor codes.
    {"Please enter the SMS verification code:", CoreOutputEvent::SmsCodeWithSkipOption, true, false},
    {"Please enter your SMS code:", CoreOutputEvent::SmsCode, true, false},
    {"Please enter your TOTP code:", CoreOutputEvent::TotpCode, true, false},
    {"Please enter the TOTP token:", CoreOutputEvent::TotpCode, true, false},
    {"Please enter rand code:", CoreOutputEvent::RandCode, true, false},
    {"Please enter the RADIUS token:", CoreOutputEvent::RadiusCodeWithSkipOption, true, false},
    // Callback URL: the only prompt whose wording differs between the two
    // protocols is the capitalisation of "url", so this rule ignores case.
    {"Please enter the callback url:", CoreOutputEvent::SsoCallback, true, true},
    // Terminal states.
    {"VPN client started", CoreOutputEvent::ClientStarted, false, false},
    {"graph check code still required after second login attempt", CoreOutputEvent::CaptchaFailed, false, false},
    {"The characters are incorrect or has expired!", CoreOutputEvent::CaptchaFailed, false, false},
    {"Access is denied.", CoreOutputEvent::AccessDenied, false, false},
    {"listen failed", CoreOutputEvent::ListenFailed, false, false},
    {"Invalid username or password!", CoreOutputEvent::InvalidCredentials, false, false},
    {"ticket is empty", CoreOutputEvent::InvalidCredentials, false, false},
    {"You are trying brute-force login on this IP address.", CoreOutputEvent::BruteForceBlocked, false, false},
    {"Login failed", CoreOutputEvent::LoginFailed, false, false},
    {"too many login failures", CoreOutputEvent::LoginFailed, false, false},
    {"unexpected newline", CoreOutputEvent::InteractiveError, false, false},
    {"auth type/login domain combination not found", CoreOutputEvent::AuthNotAvailable, false, false},
    {"invalid SID", CoreOutputEvent::AuthExpired, false, false},
    {"l3-tunnel tunnel auth failed:", CoreOutputEvent::AuthExpired, false, false},
    {"client setup error", CoreOutputEvent::ClientFailed, false, false},
    {"panic", CoreOutputEvent::CorePanic, false, false},
};

bool matchesRule(const QString &output, const OutputRule &rule)
{
    return rule.caseInsensitive
               ? output.contains(QLatin1String(rule.literal), Qt::CaseInsensitive)
               : output.contains(QLatin1String(rule.literal));
}

}

CoreOutputEvent CoreOutputParser::parse(const QString &output)
{
    for (const OutputRule &rule : kOutputRules)
    {
        if (matchesRule(output, rule))
        {
            return rule.event;
        }
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

bool CoreOutputParser::hasInteractivePrompt(const QByteArray &output)
{
    // The buffer holds raw console bytes and every literal is ASCII, so decoding
    // as Latin-1 keeps the comparison byte-wise without a lossy UTF-8 decode.
    const QString text = QString::fromLatin1(output);
    for (const OutputRule &rule : kOutputRules)
    {
        if (rule.answerRequired && matchesRule(text, rule))
        {
            return true;
        }
    }
    return false;
}
