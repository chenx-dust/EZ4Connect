#ifndef CONNECTIONSESSIONSTATE_H
#define CONNECTIONSESSIONSTATE_H

#include "connectionerror.h"

enum class ConnectionState
{
    Disconnected,
    Starting,
    Running,
    Stopping,
    Reconnecting,
    Interrupted,
    Failed,
    // The portal ended the session and the core cannot bring it back on its own:
    // nothing is running, and only a fresh login (a new process) recovers.
    ReloginRequired,
};

struct ReconnectPolicy
{
    bool enabled = false;
    // The first retry's delay. Every further attempt doubles it, which at the
    // default base is the 1/2/4/8/16 s ladder the client walks before it gives up
    // on going back online.
    int delayMs = 1000;
    // The retry budget. A failure that outlives it is terminal, so the caller can
    // ask the user instead of retrying forever.
    int maxAttempts = 5;
};

enum class ProcessFinishAction
{
    Complete,
    Reconnect,
};

class ConnectionSessionState
{
public:
    ConnectionState state() const;
    ZJU_ERROR error() const;
    bool isActive() const;
    bool wantsConnection() const;
    // The delay of the retry about to happen, its 1-based number in the budget,
    // and that budget.
    int reconnectDelayMs() const;
    int reconnectAttempt() const;
    int reconnectAttemptLimit() const;

    bool requestStart(const ReconnectPolicy &policy);
    void connectionEstablished();
    void recordError(ZJU_ERROR error);
    // The core reported that the portal ended the session: a terminal outcome,
    // not a reconnect target. Returns false when there is no live session to end
    // (nothing started, already stopping, or the user asked to disconnect).
    bool sessionExpired(int code);
    int reloginCode() const;
    bool requestStop();
    ProcessFinishAction processFinished();
    void beginReconnect();

private:
    static bool isReconnectable(ZJU_ERROR error);

    ConnectionState currentState = ConnectionState::Disconnected;
    ZJU_ERROR currentError = ZJU_ERROR::NONE;
    int currentReloginCode = 0;
    ReconnectPolicy reconnectPolicy;
    // Retries scheduled for the current outage; a live connection arms it again.
    int reconnectAttempts = 0;
    bool desiredConnected = false;
};

#endif // CONNECTIONSESSIONSTATE_H
