#ifndef COREPROCESS_H
#define COREPROCESS_H

#include <QObject>

#include "core/connectionerror.h"
#include "core/connectionprofile.h"
#include "core/sessionstatus.h"

class CoreProcess : public QObject
{
    Q_OBJECT

public:
    explicit CoreProcess(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~CoreProcess() override = default;

    virtual void start(const ConnectionProfile &profile) = 0;
    virtual void stop() = 0;
    virtual void writeInput(const QByteArray &data) = 0;

signals:
    void error(ZJU_ERROR error);
    void outputRead(const QString &output);
    // The core says the portal ended the session and no recovery is left inside
    // the running process; `code` is the portal's own code when it printed one.
    void sessionRequiresRelogin(int code);
    // What the session maintenance and the keep-alive report about themselves.
    void sessionStateReported(const SessionStateLine &line);
    void keepAliveReported(const KeepAliveLine &line);
    void graphCaptcha(const QString &graphFile);
    void smsCode(bool showSkipSecondaryAuthOption);
    void totpCode();
    void randCode();
    void radiusCode(bool showSkipSecondaryAuthOption);
    void ssoAuth();
    void askSudoPass();
    void started();
    void connectionEstablished();
    void finished();
};

#endif // COREPROCESS_H
