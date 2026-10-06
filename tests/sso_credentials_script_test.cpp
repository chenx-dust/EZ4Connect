#include <QApplication>
#include <QDebug>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QPointer>
#include <QTimer>
#include <QVariant>
#include <QWebEnginePage>
#include <QWebEngineProfile>
#include <QWebEngineUrlRequestInfo>
#include <QWebEngineUrlRequestInterceptor>
#include <QWebEngineView>

#include <memory>

namespace
{
const QUrl trustedUrl(QStringLiteral("https://iam.example.test/login"));
const QString trustedOrigin(QStringLiteral("https://iam.example.test"));
const QString syntheticUsername(QString::fromUtf8("测试用户-α@example.test"));
const QString syntheticPassword(QString::fromUtf8("synthetic-'\\\"-密码-\\-<>&"));

// Every page in this test is supplied by setHtml. Never contact an SSO server.
class BlockNetwork final : public QWebEngineUrlRequestInterceptor
{
public:
    void interceptRequest(QWebEngineUrlRequestInfo &request) override
    {
        if (request.requestUrl().scheme() == QStringLiteral("https")
            || request.requestUrl().scheme() == QStringLiteral("http"))
        {
            request.block(true);
        }
    }
};

class Browser
{
public:
    Browser()
    {
        profile.setUrlRequestInterceptor(&networkBlocker);
        view.setPage(new QWebEnginePage(&profile, &view));
        view.resize(800, 600);
        view.show();
    }

    bool load(const QString &body, const QUrl &url = trustedUrl)
    {
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        bool completed = false;
        bool succeeded = false;
        QObject::connect(view.page(), &QWebEnginePage::loadFinished, &loop,
                         [&](bool ok) {
                             completed = true;
                             succeeded = ok;
                             loop.quit();
                         });
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(10000);
        view.setHtml(QStringLiteral("<!doctype html><html><body>") + body
                         + QStringLiteral("</body></html>"), url);
        loop.exec();
        if (!completed || !succeeded)
        {
            qCritical() << "Synthetic SSO document did not finish loading";
            return false;
        }
        return true;
    }

    bool evaluate(const QString &javascript, QJsonObject &result)
    {
        struct Result
        {
            bool completed = false;
            QVariant value;
        };
        const auto state = std::make_shared<Result>();
        QEventLoop loop;
        const QPointer<QEventLoop> guardedLoop(&loop);
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(5000);
        view.page()->runJavaScript(QStringLiteral("JSON.stringify(") + javascript
                                      + QStringLiteral(")"),
                                  [state, guardedLoop](const QVariant &value) {
                                      state->value = value;
                                      state->completed = true;
                                      if (guardedLoop) guardedLoop->quit();
                                  });
        loop.exec();
        const QJsonDocument document = QJsonDocument::fromJson(
            state->value.toString().toUtf8());
        if (!state->completed || !document.isObject())
        {
            qCritical() << "Synthetic SSO JavaScript evaluation failed or timed out";
            return false;
        }
        result = document.object();
        return true;
    }

    bool request(const QString &script, const QString &operation,
                 QJsonObject &result, const QString &origin = trustedOrigin)
    {
        const QJsonObject request{{QStringLiteral("operation"), operation},
                                  {QStringLiteral("origin"), origin},
                                  {QStringLiteral("username"), syntheticUsername},
                                  {QStringLiteral("password"), syntheticPassword}};
        return evaluate(QStringLiteral("(") + script + QStringLiteral(")(")
                            + QString::fromUtf8(QJsonDocument(request).toJson(
                                QJsonDocument::Compact)) + QStringLiteral(")"),
                        result);
    }

private:
    QWebEngineProfile profile;
    BlockNetwork networkBlocker;
    QWebEngineView view;
};

QString loginForm(const QString &usernameAttributes = {},
                  const QString &passwordAttributes = {},
                  const QString &formAttributes = {})
{
    return QStringLiteral("<form id='login' %1><input id='username' name='username' "
                          "autocomplete='username' %2><input id='password' "
                          "type='password' autocomplete='current-password' %3>"
                          "<button type='submit'>Sign in</button></form>")
        .arg(formAttributes, usernameAttributes, passwordAttributes);
}

bool expectStatus(Browser &browser, const QString &script, const QString &operation,
                  const QString &status, const QString &context,
                  const QString &origin = trustedOrigin)
{
    QJsonObject result;
    if (!browser.request(script, operation, result, origin)) return false;
    if (result.value(QStringLiteral("status")).toString() != status)
    {
        qCritical() << context << "returned" << result.value(QStringLiteral("status"))
                    << "instead of" << status;
        return false;
    }
    return true;
}

bool fillsAndCapturesRealDom(Browser &browser, const QString &script)
{
    const QString handlers = QStringLiteral(R"(
        <script>
        window.events = [];
        window.submits = 0;
        window.frameworkState = {};
        document.getElementById('login').addEventListener('submit', e => {
            e.preventDefault(); window.submits++;
        });
        ['username', 'password'].forEach(id => {
            ['input', 'change'].forEach(kind => {
                document.getElementById(id).addEventListener(kind, e => {
                    window.events.push(id + ':' + kind);
                    window.frameworkState[id] = e.target.value;
                });
            });
        });
        </script>
    )");
    if (!browser.load(loginForm() + handlers)
        || !expectStatus(browser, script, QStringLiteral("inspect"),
                         QStringLiteral("ready"), QStringLiteral("Login inspection"))
        || !expectStatus(browser, script, QStringLiteral("fill"),
                         QStringLiteral("filled"), QStringLiteral("DOM autofill")))
        return false;

    QJsonObject dom;
    if (!browser.evaluate(QStringLiteral(R"(({
        username: document.getElementById('username').value,
        password: document.getElementById('password').value,
        events: window.events.join(','),
        stateUser: window.frameworkState.username,
        statePassword: window.frameworkState.password,
        submits: window.submits
    }))"), dom)) return false;
    if (dom.value(QStringLiteral("username")).toString() != syntheticUsername
        || dom.value(QStringLiteral("password")).toString() != syntheticPassword
        || dom.value(QStringLiteral("stateUser")).toString() != syntheticUsername
        || dom.value(QStringLiteral("statePassword")).toString() != syntheticPassword
        || dom.value(QStringLiteral("events")).toString()
            != QStringLiteral("username:input,username:change,password:input,password:change")
        || dom.value(QStringLiteral("submits")).toInt() != 0)
    {
        qCritical() << "Autofill did not preserve Unicode, quoting, field events, or manual submit";
        return false;
    }

    if (!expectStatus(browser, script, QStringLiteral("fill"),
                      QStringLiteral("already-filled"), QStringLiteral("Repeat autofill")))
        return false;
    // Capture must read the current fields, even after the user edits the autofill.
    const QString editedUsername = syntheticUsername + QStringLiteral("-edited");
    const QString editedPassword = syntheticPassword + QStringLiteral("-edited");
    const QJsonObject editedValues{{QStringLiteral("username"), editedUsername},
                                  {QStringLiteral("password"), editedPassword}};
    QJsonObject edited;
    if (!browser.evaluate(QStringLiteral("(() => { const values = ")
                              + QString::fromUtf8(QJsonDocument(editedValues).toJson(
                                  QJsonDocument::Compact))
                              + QStringLiteral("; document.getElementById('username').value = values.username; "
                                               "document.getElementById('password').value = values.password; "
                                               "return {edited: true}; })()"), edited)) return false;
    QJsonObject captured;
    if (!browser.request(script, QStringLiteral("capture"), captured)) return false;
    if (captured.value(QStringLiteral("status")).toString() != QStringLiteral("captured")
        || captured.value(QStringLiteral("username")).toString() != editedUsername
        || captured.value(QStringLiteral("password")).toString() != editedPassword)
    {
        qCritical() << "Capture did not read the actual filled DOM values";
        return false;
    }
    return true;
}

bool handlesDelayedForm(Browser &browser, const QString &script)
{
    if (!browser.load(QStringLiteral("<div id='mount'></div>"))
        || !expectStatus(browser, script, QStringLiteral("inspect"),
                         QStringLiteral("no-login-form"), QStringLiteral("Unrendered form")))
        return false;
    const QJsonObject payload{{QStringLiteral("html"), loginForm()}};
    const QString literal = QString::fromUtf8(QJsonDocument(payload).toJson(
        QJsonDocument::Compact));
    QJsonObject inserted;
    if (!browser.evaluate(QStringLiteral("(() => { const payload = ") + literal
                              + QStringLiteral("; document.getElementById('mount').innerHTML = "
                                               "payload.html; return {inserted: true}; })()"),
                          inserted)) return false;
    return expectStatus(browser, script, QStringLiteral("capture"),
                        QStringLiteral("empty"), QStringLiteral("Empty form capture"))
        && expectStatus(browser, script, QStringLiteral("inspect"),
                        QStringLiteral("ready"), QStringLiteral("Delayed form inspection"))
        && expectStatus(browser, script, QStringLiteral("fill"),
                        QStringLiteral("filled"), QStringLiteral("Delayed form autofill"));
}

bool preservesAnotherAccount(Browser &browser, const QString &script)
{
    if (!browser.load(loginForm(QStringLiteral("value='different-account'")))
        || !expectStatus(browser, script, QStringLiteral("fill"),
                         QStringLiteral("different-account"), QStringLiteral("Different account")))
        return false;
    QJsonObject dom;
    if (!browser.evaluate(QStringLiteral("({username: document.getElementById('username').value, "
                                         "password: document.getElementById('password').value})"),
                          dom)) return false;
    if (dom.value(QStringLiteral("username")).toString() != QStringLiteral("different-account")
        || !dom.value(QStringLiteral("password")).toString().isEmpty())
    {
        qCritical() << "Autofill overwrote another account or supplied its password";
        return false;
    }
    return true;
}

bool rejectsUnsafeForms(Browser &browser, const QString &script)
{
    const QList<QPair<QString, QString>> cases{
        {QStringLiteral("Hidden password"), loginForm({}, QStringLiteral("style='display:none'"))},
        {QStringLiteral("Hidden form"), loginForm({}, {}, QStringLiteral("style='display:none'"))},
        {QStringLiteral("Read-only password"), loginForm({}, QStringLiteral("readonly"))},
        {QStringLiteral("Disabled password"), loginForm({}, QStringLiteral("disabled"))},
        {QStringLiteral("OTP password"), loginForm({}, QStringLiteral("name='one-time-password'"))},
        {QStringLiteral("Multiple password fields"), loginForm()
            + QStringLiteral("<input type='password' id='other-password'>")},
        {QStringLiteral("New password"), QStringLiteral("<form><input autocomplete='username'>"
            "<input type='password' autocomplete='new-password'></form>")},
        {QStringLiteral("OTP username candidate"), QStringLiteral("<form><input type='text' "
            "autocomplete='one-time-code' name='otp'><input type='password'></form>")},
        {QStringLiteral("Ambiguous usernames"), QStringLiteral("<form><input autocomplete='username'>"
            "<input autocomplete='username'><input type='password'></form>")}
    };
    for (const auto &test : cases)
    {
        if (!browser.load(test.second)
            || !expectStatus(browser, script, QStringLiteral("fill"),
                             QStringLiteral("no-login-form"), test.first))
            return false;
        QJsonObject dom;
        if (!browser.evaluate(QStringLiteral("({empty: Array.from(document.querySelectorAll('input'))"
                                             ".every(input => input.value === '')})"), dom)
            || !dom.value(QStringLiteral("empty")).toBool())
        {
            qCritical() << test.first << "received synthetic credentials";
            return false;
        }
    }
    return true;
}

bool restrictsOrigins(Browser &browser, const QString &script)
{
    if (!browser.load(loginForm({}, {}, QStringLiteral("action='https://other.example.test/post'")))
        || !expectStatus(browser, script, QStringLiteral("fill"),
                         QStringLiteral("origin-mismatch"), QStringLiteral("Cross-origin action")))
        return false;
    const QList<QPair<QString, QString>> overriddenActions{
        {QStringLiteral("Submit button action"), loginForm()
            + QStringLiteral("<button type='submit' form='login' "
                             "formaction='https://other.example.test/post'>Sign in</button>")},
        {QStringLiteral("Submit input action"), loginForm()
            + QStringLiteral("<input type='submit' form='login' "
                             "formaction='https://other.example.test/post'>")},
        {QStringLiteral("Image input action"), loginForm()
            + QStringLiteral("<input type='image' form='login' "
                             "formaction='https://other.example.test/post'>")}
    };
    for (const auto &test : overriddenActions)
    {
        if (!browser.load(test.second)) return false;
        for (const QString &operation : {QStringLiteral("inspect"), QStringLiteral("capture"),
                                         QStringLiteral("fill")})
            if (!expectStatus(browser, script, operation, QStringLiteral("origin-mismatch"),
                              test.first)) return false;
    }
    // A same-origin override remains a supported login form.
    if (!browser.load(loginForm()
            + QStringLiteral("<button type='submit' form='login' formaction='/alternate-login'>Sign in</button>"))
        || !expectStatus(browser, script, QStringLiteral("fill"),
                         QStringLiteral("filled"), QStringLiteral("Same-origin submit override")))
        return false;
    if (!browser.load(loginForm())
        || !expectStatus(browser, script, QStringLiteral("fill"),
                         QStringLiteral("origin-mismatch"), QStringLiteral("Wrong trusted origin"),
                         QStringLiteral("https://other.example.test")))
        return false;
    return browser.load(loginForm(), QUrl(QStringLiteral("http://iam.example.test/login")))
        && expectStatus(browser, script, QStringLiteral("fill"),
                        QStringLiteral("origin-mismatch"), QStringLiteral("Insecure HTTP"),
                        QStringLiteral("http://iam.example.test"));
}

bool ignoresFrames(Browser &browser, const QString &script)
{
    const QString iframe = QStringLiteral("<iframe id='embedded' srcdoc=\"")
        + loginForm().toHtmlEscaped() + QStringLiteral("\"></iframe>");
    if (!browser.load(iframe)
        || !expectStatus(browser, script, QStringLiteral("fill"),
                         QStringLiteral("no-login-form"), QStringLiteral("Iframe-only login")))
        return false;
    QJsonObject dom;
    if (!browser.evaluate(QStringLiteral(R"((() => {
        const child = document.getElementById('embedded').contentDocument;
        return {empty: child.getElementById('username').value === ''
            && child.getElementById('password').value === ''};
    })())"), dom)) return false;
    if (!dom.value(QStringLiteral("empty")).toBool())
    {
        qCritical() << "Main frame autofill changed an embedded frame";
        return false;
    }
    const QJsonObject request{{QStringLiteral("operation"), QStringLiteral("fill")},
                              {QStringLiteral("origin"), trustedOrigin},
                              {QStringLiteral("username"), syntheticUsername},
                              {QStringLiteral("password"), syntheticPassword}};
    const QString childCall = QStringLiteral("(") + script + QStringLiteral(")(")
        + QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact))
        + QStringLiteral(")");
    const QJsonObject payload{{QStringLiteral("script"), childCall}};
    QJsonObject childResult;
    if (!browser.evaluate(QStringLiteral("(() => { const payload = ")
                              + QString::fromUtf8(QJsonDocument(payload).toJson(
                                  QJsonDocument::Compact))
                              + QStringLiteral("; return document.getElementById('embedded')"
                                               ".contentWindow.eval(payload.script); })()"),
                          childResult)) return false;
    if (childResult.value(QStringLiteral("status")).toString()
        != QStringLiteral("origin-mismatch"))
    {
        qCritical() << "Credential script accepted execution inside an embedded frame";
        return false;
    }
    return true;
}

bool rejectsFormChangedDuringFill(Browser &browser, const QString &script)
{
    const QList<QPair<QString, QString>> mutations{
        {QStringLiteral("Changed form action"), QStringLiteral(
            "document.getElementById('login').action = 'https://other.example.test/post';")},
        {QStringLiteral("Changed password type"), QStringLiteral(
            "document.getElementById('password').type = 'text';")},
        {QStringLiteral("Changed password purpose"), QStringLiteral(
            "document.getElementById('password').autocomplete = 'new-password';")},
        {QStringLiteral("Changed submit action"), QStringLiteral(
            "document.querySelector('button').setAttribute('formaction', 'https://other.example.test/post');")},
        {QStringLiteral("Replaced username"), QStringLiteral(
            "const old = document.getElementById('username'); old.replaceWith(old.cloneNode(true));")},
        {QStringLiteral("Disabled username"), QStringLiteral(
            "document.getElementById('username').disabled = true;")},
        {QStringLiteral("Changed username type"), QStringLiteral(
            "document.getElementById('username').type = 'hidden';")},
        {QStringLiteral("Moved username to another form"), QStringLiteral(
            "const form = document.createElement('form'); document.body.appendChild(form); "
            "form.appendChild(document.getElementById('username')); ")},
        {QStringLiteral("Moved password to another form"), QStringLiteral(
            "const form = document.createElement('form'); document.body.appendChild(form); "
            "form.appendChild(document.getElementById('password')); ")}
    };
    for (const auto &mutation : mutations)
    {
        const QString handler = QStringLiteral("<script>document.getElementById('username')"
            ".addEventListener('input', () => { ") + mutation.second
            + QStringLiteral(" });</script>");
        if (!browser.load(loginForm() + handler)) return false;
        QJsonObject result;
        if (!browser.request(script, QStringLiteral("fill"), result)) return false;
        const QString status = result.value(QStringLiteral("status")).toString();
        if (status != QStringLiteral("form-changed") && status != QStringLiteral("origin-mismatch"))
        {
            qCritical() << mutation.first << "during autofill was not rejected";
            return false;
        }
        QJsonObject dom;
        if (!browser.evaluate(QStringLiteral("({password: document.getElementById('password').value})"), dom)
            || !dom.value(QStringLiteral("password")).toString().isEmpty())
        {
            qCritical() << mutation.first << "received a password after the form changed";
            return false;
        }
    }
    return true;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication application(argc, argv);
    QFile scriptFile(QStringLiteral(":/resource/sso-credentials.js"));
    if (!scriptFile.open(QIODevice::ReadOnly))
    {
        qCritical() << "SSO credential script resource was not included in the test target";
        return 1;
    }
    const QString script = QString::fromUtf8(scriptFile.readAll());
    Browser browser;
    return fillsAndCapturesRealDom(browser, script)
        && handlesDelayedForm(browser, script)
        && preservesAnotherAccount(browser, script)
        && rejectsUnsafeForms(browser, script)
        && restrictsOrigins(browser, script)
        && ignoresFrames(browser, script)
        && rejectsFormChangedDuringFill(browser, script)
        ? 0 : 1;
}
