#include <QCoreApplication>
#include <QDebug>
#include <QList>

#include "infrastructure/coreprocess/coreoutputparser.h"

namespace
{
// Every literal the core can print, with the file that prints it. This table is
// the contract between the core's stdout and the dialogs: a prompt the core
// grows has to be added here, and a prompt that stops matching fails the suite
// instead of hanging a login in the "Starting" state.
//
// The challenge handler prints "<message> " - with a trailing space and no
// newline - hence the trailing spaces below on the aTrust prompts.
struct ParserCase
{
    const char *literal; // exactly what the core prints
    const char *source;  // where the core prints it
    CoreOutputEvent expected;
    bool interactive; // the core blocks on stdin until the answer arrives
};

bool matchesEveryCoreLiteral()
{
    const QList<ParserCase> cases{
        {"SUDO_ASK_PASS", "askpass helper", CoreOutputEvent::AskSudoPassword, true},
        {"Graph check code saved to /tmp/graph.jpg",
         "cores that report the captcha file themselves", CoreOutputEvent::GraphCaptcha, false},
        {"Please enter the graph check code JSON: ",
         "client/atrust/auth/auth.go withGraphCheckCode", CoreOutputEvent::GraphCaptcha, true},
        {"Please enter the SMS verification code: ",
         "client/atrust/auth/sms_check_code.go", CoreOutputEvent::SmsCodeWithSkipOption, true},
        {"Please enter your SMS code:", "client/easyconnect/request.go", CoreOutputEvent::SmsCode, true},
        {"Please enter your TOTP code:", "client/easyconnect/request.go", CoreOutputEvent::TotpCode, true},
        {"Please enter the TOTP token: ", "client/atrust/auth/token.go", CoreOutputEvent::TotpCode, true},
        {"Please enter rand code:", "client/easyconnect/request.go", CoreOutputEvent::RandCode, true},
        {"Please enter the RADIUS token: ",
         "client/atrust/auth/token.go", CoreOutputEvent::RadiusCodeWithSkipOption, true},
        {"Please enter the callback url:", "client/atrust/auth/cas.go", CoreOutputEvent::SsoCallback, true},
        {"Please enter the callback URL: ",
         "client/atrust/auth/https_oauth2.go", CoreOutputEvent::SsoCallback, true},
        {"VPN client started", "client/atrust/client.go", CoreOutputEvent::ClientStarted, false},
        {"graph check code still required after second login attempt",
         "client/atrust/auth", CoreOutputEvent::CaptchaFailed, false},
        {"The characters are incorrect or has expired!",
         "client/atrust/auth", CoreOutputEvent::CaptchaFailed, false},
        {"Access is denied.", "client/atrust/auth", CoreOutputEvent::AccessDenied, false},
        {"listen failed on 127.0.0.1:1080", "core startup", CoreOutputEvent::ListenFailed, false},
        {"Invalid username or password!",
         "client/atrust/auth", CoreOutputEvent::InvalidCredentials, false},
        {"ticket is empty", "client/atrust/auth", CoreOutputEvent::InvalidCredentials, false},
        {"You are trying brute-force login on this IP address.",
         "client/atrust/auth", CoreOutputEvent::BruteForceBlocked, false},
        {"Login failed", "client/atrust/auth", CoreOutputEvent::LoginFailed, false},
        {"too many login failures", "client/atrust/auth", CoreOutputEvent::LoginFailed, false},
        {"unexpected newline", "core stdin reader", CoreOutputEvent::InteractiveError, false},
        {"auth type/login domain combination not found",
         "client/atrust/auth", CoreOutputEvent::AuthNotAvailable, false},
        {"invalid SID", "client/atrust/auth", CoreOutputEvent::AuthExpired, false},
        {"l3-tunnel tunnel auth failed:", "client/atrust/l3tunnel.go", CoreOutputEvent::AuthExpired, false},
        {"client setup error", "client/atrust", CoreOutputEvent::ClientFailed, false},
        {"panic: runtime error", "core runtime", CoreOutputEvent::CorePanic, false},
        {"ordinary core output", "", CoreOutputEvent::None, false},
    };

    for (const ParserCase &testCase : cases)
    {
        const QString output = QString::fromUtf8(testCase.literal);
        const CoreOutputEvent actual = CoreOutputParser::parse(output);
        if (actual != testCase.expected)
        {
            qCritical() << "matchesEveryCoreLiteral failed for" << testCase.source << testCase.literal
                        << "expected event" << static_cast<int>(testCase.expected)
                        << "actual" << static_cast<int>(actual);
            return false;
        }

        const bool interactive = CoreOutputParser::hasInteractivePrompt(testCase.literal);
        if (interactive != testCase.interactive)
        {
            qCritical() << "matchesEveryCoreLiteral failed for" << testCase.source << testCase.literal
                        << "expected interactive" << testCase.interactive
                        << "actual" << interactive;
            return false;
        }
    }
    return true;
}

bool preservesExistingFirstMatchPrecedence()
{
    const QString combinedOutput = "Please enter your TOTP code:\npanic: later line";
    const CoreOutputEvent actual = CoreOutputParser::parse(combinedOutput);
    if (actual != CoreOutputEvent::TotpCode)
    {
        qCritical() << "preservesExistingFirstMatchPrecedence failed";
        return false;
    }
    return true;
}

bool recognizesInteractivePromptsBeforeNewline()
{
    // The core prints a prompt without a newline and waits: the parser has to
    // recognise it inside a partial output buffer, or the dialog never appears.
    const QList<QByteArray> prompts{
        "SUDO_ASK_PASS",
        "Please enter the graph check code JSON: ",
        "Please enter the SMS verification code: ",
        "Please enter your SMS code:",
        "Please enter your TOTP code:",
        "Please enter the TOTP token: ",
        "Please enter rand code:",
        "Please enter the RADIUS token: ",
        "Please enter the callback url:",
        "Please enter the callback URL: ",
    };
    for (const QByteArray &prompt : prompts)
    {
        if (!CoreOutputParser::hasInteractivePrompt("prefix " + prompt))
        {
            qCritical() << "recognizesInteractivePromptsBeforeNewline failed for" << prompt;
            return false;
        }
    }

    return !CoreOutputParser::hasInteractivePrompt("ordinary partial output")
           && !CoreOutputParser::hasInteractivePrompt("VPN client started");
}

bool extractsGraphCaptchaFileFromCoreOutput()
{
    return CoreOutputParser::graphCaptchaFile(
               "2026/08/02 Graph check code saved to C:\\Users\\alice\\graph_code.jpg"
           ) == "C:\\Users\\alice\\graph_code.jpg"
        && CoreOutputParser::graphCaptchaFile("ordinary output").isEmpty();
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    return matchesEveryCoreLiteral()
               && preservesExistingFirstMatchPrecedence()
               && recognizesInteractivePromptsBeforeNewline()
               && extractsGraphCaptchaFileFromCoreOutput()
           ? 0
           : 1;
}
