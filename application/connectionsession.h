#ifndef CONNECTIONSESSION_H
#define CONNECTIONSESSION_H

#include <QObject>
#include <QTimer>

#include "application/coreprocess.h"
#include "core/connectionprofile.h"
#include "core/connectionsessionstate.h"
#include "core/sessionstatus.h"

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
    // The portal's code from the last "requires reauthentication" line, or 0.
    int reloginCode() const;
    // What the core last reported about the session and about its keep-alive.
    const SessionStatus &sessionStatus() const;

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
    void sessionStatusChanged();
    // The core's own keep-alive report, forwarded: the UI decides what deserves a
    // notification, the session only keeps the status for display.
    void keepAliveReported(const KeepAliveLine &line);
    void finished(ZJU_ERROR error);

private:
    void startCore();
    void handleCoreFinished();
    void beginRelogin(int code);
    void handleSessionRequiresRelogin(int code);
    void handleSessionStateReported(const SessionStateLine &line);
    void handleKeepAliveReported(const KeepAliveLine &line);
    void handleSudoPasswordRequest();

    CoreProcess *coreProcess;
    QTimer reconnectTimer;
    ConnectionSessionState sessionState;
    SessionStatus currentSessionStatus;
    ConnectionProfile currentProfile;
    QString savedSudoPassword;
    bool sudoPasswordSubmitted = false;
};

#endif // CONNECTIONSESSION_H
