#include <QCoreApplication>
#include <QDebug>
#include <QList>

#include "infrastructure/coreprocess/coreoutputparser.h"

namespace
{
struct ParserCase
{
    const char *output;
    CoreOutputEvent expected;
};

bool recognizesCoreProtocol()
{
    const QList<ParserCase> cases{
        {"SUDO_ASK_PASS", CoreOutputEvent::AskSudoPassword},
        {"Graph check code saved to /tmp/graph.jpg", CoreOutputEvent::GraphCaptcha},
        {"Please enter the SMS verification code: ", CoreOutputEvent::SmsCodeWithSkipOption},
        {"Please enter your SMS code:", CoreOutputEvent::SmsCode},
        {"Please enter your TOTP code:", CoreOutputEvent::TotpCode},
        {"Please enter the callback url:", CoreOutputEvent::SsoCallback},
        {"VPN client started", CoreOutputEvent::ClientStarted},
        {"graph check code still required after second login attempt", CoreOutputEvent::CaptchaFailed},
        {"Access is denied.", CoreOutputEvent::AccessDenied},
        {"listen failed on 127.0.0.1", CoreOutputEvent::ListenFailed},
        {"Invalid username or password!", CoreOutputEvent::InvalidCredentials},
        {"ticket is empty", CoreOutputEvent::InvalidCredentials},
        {"You are trying brute-force login on this IP address.", CoreOutputEvent::BruteForceBlocked},
        {"Login failed", CoreOutputEvent::LoginFailed},
        {"too many login failures", CoreOutputEvent::LoginFailed},
        {"unexpected newline", CoreOutputEvent::InteractiveError},
        {"auth type/login domain combination not found", CoreOutputEvent::AuthNotAvailable},
        {"invalid SID", CoreOutputEvent::AuthExpired},
        {"l3-tunnel tunnel auth failed:", CoreOutputEvent::AuthExpired},
        // The core's session ladder kept the process alive; this is the line that
        // says a login is the only way back.
        {"aTrust session requires reauthentication (code=75500002)",
         CoreOutputEvent::SessionRequiresRelogin},
        {"aTrust session state: online reason=logged in code=0",
         CoreOutputEvent::SessionStateReported},
        {"KeepAlive: OK status=200 target=http://intranet.example/ routed=tunnel",
         CoreOutputEvent::KeepAliveReported},
        {"client setup error", CoreOutputEvent::ClientFailed},
        {"panic: runtime error", CoreOutputEvent::CorePanic},
        {"ordinary core output", CoreOutputEvent::None},
    };

    for (const ParserCase &testCase : cases)
    {
        const CoreOutputEvent actual = CoreOutputParser::parse(QString::fromUtf8(testCase.output));
        if (actual != testCase.expected)
        {
            qCritical() << "recognizesCoreProtocol failed for" << testCase.output
                        << "expected" << static_cast<int>(testCase.expected)
                        << "actual" << static_cast<int>(actual);
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
    const QList<QByteArray> prompts{
        "SUDO_ASK_PASS",
        "Please enter the SMS verification code: ",
        "Please enter your SMS code:",
        "Please enter your TOTP code:",
        "Please enter rand code:",
        "Please enter the callback url:",
    };
    for (const QByteArray &prompt : prompts)
    {
        if (!CoreOutputParser::hasInteractivePrompt("prefix " + prompt))
        {
            qCritical() << "recognizesInteractivePromptsBeforeNewline failed for" << prompt;
            return false;
        }
    }

    return !CoreOutputParser::hasInteractivePrompt("ordinary partial output");
}

bool extractsGraphCaptchaFileFromCoreOutput()
{
    return CoreOutputParser::graphCaptchaFile(
               "2026/08/02 Graph check code saved to C:\\Users\\alice\\graph_code.jpg"
           ) == "C:\\Users\\alice\\graph_code.jpg"
        && CoreOutputParser::graphCaptchaFile("ordinary output").isEmpty();
}

bool extractsSessionReloginCodeAndNeverTreatsItAsAPrompt()
{
    const QByteArray reloginLine =
        "2026/10/18 12:00:00 aTrust session requires reauthentication (code=75500002)";
    if (CoreOutputParser::sessionReloginCode(QString::fromUtf8(reloginLine)) != 75500002
        || CoreOutputParser::sessionReloginCode(
               "aTrust session requires reauthentication"
           ) != 0
        || CoreOutputParser::sessionReloginCode("ordinary output") != 0)
    {
        qCritical() << "extractsSessionReloginCodeAndNeverTreatsItAsAPrompt failed on the code";
        return false;
    }

    // The core is not waiting on stdin: answering this line would feed the login
    // prompt's input to a process that asked for nothing.
    return !CoreOutputParser::hasInteractivePrompt(reloginLine);
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    return recognizesCoreProtocol()
               && preservesExistingFirstMatchPrecedence()
               && recognizesInteractivePromptsBeforeNewline()
               && extractsGraphCaptchaFileFromCoreOutput()
               && extractsSessionReloginCodeAndNeverTreatsItAsAPrompt()
           ? 0
           : 1;
}
