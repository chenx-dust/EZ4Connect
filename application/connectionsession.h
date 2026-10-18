#ifndef CONNECTIONSESSION_H
#define CONNECTIONSESSION_H

#include <QObject>
#include <QTimer>

#include "application/coreprocess.h"
#include "core/connectionprofile.h"
#include "core/connectionsessionstate.h"

class ConnectionSession : public QObject
{
Q_OBJECT

public:
    explicit ConnectionSession(CoreProcess *coreProcess, QObject *parent = nullptr);

    bool start(const ConnectionProfile &profile, const ReconnectPolicy &policy);
    void stop();
    void submitInput(const QByteArray &data);
    void cancelInteractiveInput();
    void submitSudoPassword(const QString &password, bool remember);

    ConnectionState state() const;
    bool isActive() const;
    // The ladder the reconnect timer is walking: the pending retry's delay, its
    // number, and the budget it is counted against.
    int reconnectDelayMs() const;
    int reconnectAttempt() const;
    int reconnectAttemptLimit() const;
    // The portal's code from the last "requires reauthentication" line, or 0.
    int reloginCode() const;

signals:
    void stateChanged(ConnectionState state);
    void outputRead(const QString &output);
    void graphCaptcha(const QString &graphFile);
    void smsCode(bool showSkipSecondaryAuthOption);
    void totpCode();
    void randCode();
    void radiusCode(bool showSkipSecondaryAuthOption);
    void ssoAuth();
    void askSudoPass();
    void savedSudoPasswordRejected();
    void reconnectScheduled(int delayMs);
    void reloginRequired(int code);
    void finished(ZJU_ERROR error);

private:
    void startCore();
    void handleCoreFinished();
    void handleSessionRequiresRelogin(int code);
    void handleSudoPasswordRequest();

    CoreProcess *coreProcess;
    QTimer reconnectTimer;
    ConnectionSessionState sessionState;
    ConnectionProfile currentProfile;
    QString savedSudoPassword;
    bool sudoPasswordSubmitted = false;
};

#endif // CONNECTIONSESSION_H
