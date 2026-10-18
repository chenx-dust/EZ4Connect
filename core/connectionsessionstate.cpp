#include "connectionsessionstate.h"

#include <limits>

ConnectionState ConnectionSessionState::state() const
{
    return currentState;
}

ZJU_ERROR ConnectionSessionState::error() const
{
    return currentError;
}

bool ConnectionSessionState::isActive() const
{
    return currentState == ConnectionState::Starting
        || currentState == ConnectionState::Running
        || currentState == ConnectionState::Stopping
        || currentState == ConnectionState::Reconnecting;
}

bool ConnectionSessionState::wantsConnection() const
{
    return desiredConnected;
}

int ConnectionSessionState::reconnectDelayMs() const
{
    // The ladder doubles from the configured first delay. The budget bounds the
    // number of steps, and the loop guard bounds the value itself: a delay near
    // INT_MAX would otherwise overflow into a negative timeout.
    int delay = reconnectPolicy.delayMs > 0 ? reconnectPolicy.delayMs : 1;
    for (int attempt = 1; attempt < reconnectAttempts; ++attempt)
    {
        if (delay > std::numeric_limits<int>::max() / 2)
        {
            break;
        }
        delay *= 2;
    }
    return delay;
}

int ConnectionSessionState::reconnectAttempt() const
{
    return reconnectAttempts;
}

int ConnectionSessionState::reconnectAttemptLimit() const
{
    return reconnectPolicy.maxAttempts > 0 ? reconnectPolicy.maxAttempts : 1;
}

bool ConnectionSessionState::requestStart(const ReconnectPolicy &policy)
{
    if (isActive())
    {
        return false;
    }

    reconnectPolicy = policy;
    reconnectAttempts = 0;
    desiredConnected = true;
    currentError = ZJU_ERROR::NONE;
    currentState = ConnectionState::Starting;
    return true;
}

void ConnectionSessionState::connectionEstablished()
{
    if (currentState == ConnectionState::Starting)
    {
        currentState = ConnectionState::Running;
        // A connection that came back arms a fresh budget: the next outage gets
        // the whole ladder again.
        reconnectAttempts = 0;
    }
}

void ConnectionSessionState::recordError(ZJU_ERROR error)
{
    if (currentError == ZJU_ERROR::NONE)
    {
        currentError = error;
    }
}

bool ConnectionSessionState::sessionExpired(int code)
{
    if (!desiredConnected
        || (currentState != ConnectionState::Starting
            && currentState != ConnectionState::Running))
    {
        return false;
    }

    // The portal's verdict replaces an earlier error: it is what the user has to
    // act on, and the first-error rule above would hide it behind whatever was
    // recorded before.
    currentError = ZJU_ERROR::SESSION_EXPIRED;
    currentReloginCode = code;
    currentState = ConnectionState::ReloginRequired;
    return true;
}

int ConnectionSessionState::reloginCode() const
{
    return currentReloginCode;
}

bool ConnectionSessionState::requestStop()
{
    desiredConnected = false;
    if (currentState == ConnectionState::Reconnecting
        || currentState == ConnectionState::ReloginRequired)
    {
        currentState = ConnectionState::Disconnected;
        return false;
    }
    if (currentState == ConnectionState::Starting || currentState == ConnectionState::Running)
    {
        currentState = ConnectionState::Stopping;
        return true;
    }
    return currentState == ConnectionState::Stopping;
}

ProcessFinishAction ConnectionSessionState::processFinished()
{
    // A dead session is terminal: the process was stopped on purpose, the tunnel
    // is gone, and only a login brings the session back. Reconnecting here is the
    // 2.4-minute branch that repairs nothing, so the state stays where the UI can
    // act on it.
    if (currentState == ConnectionState::ReloginRequired)
    {
        desiredConnected = false;
        return ProcessFinishAction::Complete;
    }

    const bool connectionWasEstablished = currentState == ConnectionState::Running;
    if (desiredConnected
        && reconnectPolicy.enabled
        && isReconnectable(currentError)
        && reconnectAttempts < reconnectAttemptLimit())
    {
        ++reconnectAttempts;
        currentState = ConnectionState::Reconnecting;
        return ProcessFinishAction::Reconnect;
    }

    desiredConnected = false;
    if (connectionWasEstablished)
    {
        currentState = ConnectionState::Interrupted;
    }
    else
    {
        currentState = currentError == ZJU_ERROR::NONE
            ? ConnectionState::Disconnected
            : ConnectionState::Failed;
    }
    return ProcessFinishAction::Complete;
}

void ConnectionSessionState::beginReconnect()
{
    if (currentState == ConnectionState::Reconnecting && desiredConnected)
    {
        currentError = ZJU_ERROR::NONE;
        currentState = ConnectionState::Starting;
    }
}

bool ConnectionSessionState::isReconnectable(ZJU_ERROR error)
{
    // A session the portal ended is not a reconnect target: the caller offers a
    // login instead (sessionExpired), because a silent replay of the stored
    // credentials would rotate the device token and drop the account's other
    // clients.
    // CLIENT_FAILED is the core failing to set itself up: against a server it
    // cannot reach it means "failed to acquire the server manifest", i.e. the
    // network, which is what a bounded ladder should keep trying.
    return error == ZJU_ERROR::AUTH_EXPIRED
        || error == ZJU_ERROR::CLIENT_FAILED
        || error == ZJU_ERROR::OTHER;
}
