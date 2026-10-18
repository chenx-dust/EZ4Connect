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
    int delayMs = 1000;
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
    int reconnectDelayMs() const;

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
    bool desiredConnected = false;
};

#endif // CONNECTIONSESSIONSTATE_H
