#include <QCoreApplication>
#include <QDebug>

#include "core/connectionsessionstate.h"

namespace
{
bool normalLifecycle()
{
    ConnectionSessionState session;
    if (!session.requestStart({false, 1000})
        || session.state() != ConnectionState::Starting
        || !session.isActive())
    {
        qCritical() << "normalLifecycle failed at start";
        return false;
    }

    session.connectionEstablished();
    if (session.state() != ConnectionState::Running)
    {
        qCritical() << "normalLifecycle failed at running";
        return false;
    }

    if (!session.requestStop() || session.state() != ConnectionState::Stopping)
    {
        qCritical() << "normalLifecycle failed at stopping";
        return false;
    }

    if (session.processFinished() != ProcessFinishAction::Complete
        || session.state() != ConnectionState::Disconnected
        || session.isActive())
    {
        qCritical() << "normalLifecycle failed at completion";
        return false;
    }
    return true;
}

bool reconnectsOnlyEligibleFailures()
{
    ConnectionSessionState session;
    session.requestStart({true, 2500});
    session.connectionEstablished();
    session.recordError(ZJU_ERROR::AUTH_EXPIRED);

    if (session.processFinished() != ProcessFinishAction::Reconnect
        || session.state() != ConnectionState::Reconnecting
        || session.reconnectDelayMs() != 2500)
    {
        qCritical() << "reconnectsOnlyEligibleFailures failed at scheduling";
        return false;
    }

    session.beginReconnect();
    if (session.state() != ConnectionState::Starting || session.error() != ZJU_ERROR::NONE)
    {
        qCritical() << "reconnectsOnlyEligibleFailures failed at restart";
        return false;
    }

    session.recordError(ZJU_ERROR::INVALID_DETAIL);
    if (session.processFinished() != ProcessFinishAction::Complete
        || session.state() != ConnectionState::Failed)
    {
        qCritical() << "reconnectsOnlyEligibleFailures reconnected an ineligible error";
        return false;
    }
    return true;
}

bool keepsFirstErrorAndCancelsPendingReconnect()
{
    ConnectionSessionState session;
    session.requestStart({true, 1000});
    session.connectionEstablished();
    session.recordError(ZJU_ERROR::AUTH_EXPIRED);
    session.recordError(ZJU_ERROR::OTHER);
    if (session.error() != ZJU_ERROR::AUTH_EXPIRED)
    {
        qCritical() << "keepsFirstErrorAndCancelsPendingReconnect did not keep first error";
        return false;
    }

    session.processFinished();
    if (session.requestStop()
        || session.state() != ConnectionState::Disconnected
        || session.wantsConnection())
    {
        qCritical() << "keepsFirstErrorAndCancelsPendingReconnect failed to cancel reconnect";
        return false;
    }
    return true;
}

bool establishedConnectionEndsAsInterrupted()
{
    ConnectionSessionState session;
    session.requestStart({false, 1000});
    session.connectionEstablished();
    session.recordError(ZJU_ERROR::OTHER);

    if (session.processFinished() != ProcessFinishAction::Complete
        || session.state() != ConnectionState::Interrupted)
    {
        qCritical() << "establishedConnectionEndsAsInterrupted failed";
        return false;
    }
    return true;
}

bool expiredSessionWaitsForALoginInsteadOfReconnecting()
{
    ConnectionSessionState session;
    session.requestStart({true, 1000});
    session.connectionEstablished();
    // An earlier error must not hide the portal's verdict: it is the one the user
    // has to act on.
    session.recordError(ZJU_ERROR::OTHER);

    if (!session.sessionExpired(75500002)
        || session.state() != ConnectionState::ReloginRequired
        || session.isActive()
        || session.error() != ZJU_ERROR::SESSION_EXPIRED
        || session.reloginCode() != 75500002)
    {
        qCritical() << "expiredSessionWaitsForALoginInsteadOfReconnecting failed to report the session";
        return false;
    }

    // Auto-reconnect is on, and it must still not fire: on this gateway a restart
    // repairs nothing, so the state stays for the login dialog to answer.
    if (session.processFinished() != ProcessFinishAction::Complete
        || session.state() != ConnectionState::ReloginRequired
        || session.wantsConnection())
    {
        qCritical() << "expiredSessionWaitsForALoginInsteadOfReconnecting reconnected a dead session";
        return false;
    }

    return session.requestStart({true, 1000})
        && session.state() == ConnectionState::Starting;
}

bool ignoresSessionExpiryAfterDisconnectRequest()
{
    ConnectionSessionState session;
    session.requestStart({true, 1000});
    session.connectionEstablished();
    session.requestStop();

    if (session.sessionExpired(75500002)
        || session.state() != ConnectionState::Stopping)
    {
        qCritical() << "ignoresSessionExpiryAfterDisconnectRequest undid a disconnect";
        return false;
    }
    return true;
}

bool walksABoundedLadderInsteadOfRetryingForever()
{
    ConnectionSessionState session;
    session.requestStart({true, 1000});
    session.connectionEstablished();

    // The core failing to reach the server at all is the failure auto-reconnect
    // exists for: the ladder doubles the wait and counts every attempt.
    const int expectedDelays[] = {1000, 2000, 4000, 8000, 16000};
    for (int attempt = 1; attempt <= 5; ++attempt)
    {
        session.recordError(ZJU_ERROR::CLIENT_FAILED);
        if (session.processFinished() != ProcessFinishAction::Reconnect
            || session.state() != ConnectionState::Reconnecting
            || session.reconnectAttempt() != attempt
            || session.reconnectAttemptLimit() != 5
            || session.reconnectDelayMs() != expectedDelays[attempt - 1])
        {
            qCritical() << "walksABoundedLadderInsteadOfRetryingForever failed at attempt"
                        << attempt;
            return false;
        }

        session.beginReconnect();
        if (session.state() != ConnectionState::Starting
            || session.error() != ZJU_ERROR::NONE)
        {
            qCritical() << "walksABoundedLadderInsteadOfRetryingForever failed to restart";
            return false;
        }
    }

    // The budget is spent: one more failure is the end of the ladder instead of
    // another retry, and the error that ended it is the one the user is told.
    session.recordError(ZJU_ERROR::CLIENT_FAILED);
    if (session.processFinished() != ProcessFinishAction::Complete
        || session.state() != ConnectionState::Failed
        || session.wantsConnection()
        || session.error() != ZJU_ERROR::CLIENT_FAILED)
    {
        qCritical() << "walksABoundedLadderInsteadOfRetryingForever retried past its budget";
        return false;
    }
    return true;
}

bool aConnectionThatCameBackGetsTheWholeLadderAgain()
{
    ConnectionSessionState session;
    session.requestStart({true, 1000});
    session.connectionEstablished();

    session.recordError(ZJU_ERROR::OTHER);
    session.processFinished();
    session.beginReconnect();
    session.connectionEstablished();
    if (session.state() != ConnectionState::Running
        || session.reconnectAttempt() != 0)
    {
        qCritical() << "aConnectionThatCameBackGetsTheWholeLadderAgain kept the spent budget";
        return false;
    }

    session.recordError(ZJU_ERROR::OTHER);
    if (session.processFinished() != ProcessFinishAction::Reconnect
        || session.reconnectAttempt() != 1
        || session.reconnectDelayMs() != 1000)
    {
        qCritical() << "aConnectionThatCameBackGetsTheWholeLadderAgain did not restart the ladder";
        return false;
    }
    return true;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    return normalLifecycle()
        && reconnectsOnlyEligibleFailures()
        && keepsFirstErrorAndCancelsPendingReconnect()
        && establishedConnectionEndsAsInterrupted()
        && expiredSessionWaitsForALoginInsteadOfReconnecting()
        && ignoresSessionExpiryAfterDisconnectRequest()
        && walksABoundedLadderInsteadOfRetryingForever()
        && aConnectionThatCameBackGetsTheWholeLadderAgain()
        ? 0
        : 1;
}
