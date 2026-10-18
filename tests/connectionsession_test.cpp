#include <QCoreApplication>
#include <QDebug>

#include "application/connectionsession.h"

namespace
{
class FakeCoreProcess : public CoreProcess
{
public:
    int startCalls = 0;
    int stopCalls = 0;
    QByteArray lastInput;

    void start(const ConnectionProfile &) override
    {
        ++startCalls;
        emit started();
    }

    void stop() override
    {
        ++stopCalls;
    }

    void writeInput(const QByteArray &data) override
    {
        lastInput = data;
    }

    void complete()
    {
        emit finished();
    }

    void establishConnection()
    {
        emit connectionEstablished();
    }

    void requireRelogin(int code)
    {
        emit sessionRequiresRelogin(code);
    }

    void requestSudoPassword()
    {
        emit askSudoPass();
    }
};

bool delegatesProcessLifecycleThroughPort()
{
    auto *coreProcess = new FakeCoreProcess();
    ConnectionSession session(coreProcess);
    ConnectionProfile profile;
    int finishedCalls = 0;
    QObject::connect(
        &session,
        &ConnectionSession::finished,
        [&](ZJU_ERROR) { ++finishedCalls; }
    );

    if (!session.start(profile, {})
        || coreProcess->startCalls != 1
        || session.state() != ConnectionState::Starting
        || !session.isActive()
        || session.start(profile, {}))
    {
        qCritical() << "start delegation failed";
        return false;
    }

    coreProcess->establishConnection();
    if (session.state() != ConnectionState::Running)
    {
        qCritical() << "connection establishment was not reflected";
        return false;
    }

    session.submitInput("input");
    session.stop();
    if (coreProcess->lastInput != "input"
        || coreProcess->stopCalls != 1
        || session.state() != ConnectionState::Stopping)
    {
        qCritical() << "input or stop delegation failed";
        return false;
    }

    coreProcess->complete();
    if (finishedCalls != 1
        || session.state() != ConnectionState::Disconnected
        || session.isActive())
    {
        qCritical() << "completion delegation failed";
        return false;
    }
    return true;
}

bool distinguishesInterruptedConnectionFromStartupFailure()
{
    auto *coreProcess = new FakeCoreProcess();
    ConnectionSession session(coreProcess);
    ConnectionProfile profile;

    session.start(profile, {});
    coreProcess->establishConnection();
    emit coreProcess->error(ZJU_ERROR::OTHER);
    coreProcess->complete();

    if (session.state() != ConnectionState::Interrupted)
    {
        qCritical() << "established connection was reported as startup failure";
        return false;
    }
    return true;
}

bool emptySudoPasswordStopsTheSession()
{
    auto *coreProcess = new FakeCoreProcess();
    ConnectionSession session(coreProcess);
    ConnectionProfile profile;

    if (!session.start(profile, {}))
    {
        qCritical() << "failed to start session for sudo cancellation";
        return false;
    }

    coreProcess->requestSudoPassword();
    session.submitSudoPassword({}, false);
    if (coreProcess->stopCalls != 1
        || session.state() != ConnectionState::Stopping)
    {
        qCritical() << "empty sudo password did not stop the session";
        return false;
    }
    return true;
}

bool sessionExpiryStopsTheCoreAndAsksForALogin()
{
    auto *coreProcess = new FakeCoreProcess();
    ConnectionSession session(coreProcess);
    ConnectionProfile profile;
    int reloginCalls = 0;
    int reloginCode = 0;
    int reconnectCalls = 0;
    int finishedCalls = 0;
    ZJU_ERROR finishedError = ZJU_ERROR::NONE;
    QObject::connect(
        &session,
        &ConnectionSession::reloginRequired,
        [&](int code)
        {
            ++reloginCalls;
            reloginCode = code;
        }
    );
    QObject::connect(
        &session,
        &ConnectionSession::reconnectScheduled,
        [&](int) { ++reconnectCalls; }
    );
    QObject::connect(
        &session,
        &ConnectionSession::finished,
        [&](ZJU_ERROR error)
        {
            ++finishedCalls;
            finishedError = error;
        }
    );

    // Auto-reconnect is on: the dead session must still not take that branch.
    if (!session.start(profile, {true, 1000}))
    {
        qCritical() << "failed to start session for the session expiry case";
        return false;
    }
    coreProcess->establishConnection();
    coreProcess->requireRelogin(75500002);

    if (reloginCalls != 1
        || reloginCode != 75500002
        || reconnectCalls != 0
        || coreProcess->stopCalls != 1
        || session.state() != ConnectionState::ReloginRequired
        || session.isActive())
    {
        qCritical() << "session expiry did not stop the core and ask for a login";
        return false;
    }

    coreProcess->complete();
    if (finishedCalls != 1
        || finishedError != ZJU_ERROR::SESSION_EXPIRED
        || session.state() != ConnectionState::ReloginRequired
        || session.reloginCode() != 75500002)
    {
        qCritical() << "session expiry was not reported as a terminal outcome";
        return false;
    }
    return true;
}

bool cancelledInteractiveInputSubmitsNewlineBeforeStopping()
{
    auto *coreProcess = new FakeCoreProcess();
    ConnectionSession session(coreProcess);
    ConnectionProfile profile;

    if (!session.start(profile, {}))
    {
        qCritical() << "failed to start session for interactive input cancellation";
        return false;
    }

    session.cancelInteractiveInput();
    if (coreProcess->lastInput != "\r\n"
        || coreProcess->stopCalls != 1
        || session.state() != ConnectionState::Stopping)
    {
        qCritical() << "interactive input cancellation did not submit a newline and stop";
        return false;
    }
    return true;
}

bool schedulesTheLadderRungItReports()
{
    auto *coreProcess = new FakeCoreProcess();
    ConnectionSession session(coreProcess);
    ConnectionProfile profile;
    int scheduleCalls = 0;
    int scheduledDelay = 0;
    QObject::connect(
        &session,
        &ConnectionSession::reconnectScheduled,
        [&](int delayMs)
        {
            ++scheduleCalls;
            scheduledDelay = delayMs;
        }
    );

    if (!session.start(profile, {true, 1000}))
    {
        qCritical() << "failed to start session for the ladder case";
        return false;
    }
    coreProcess->establishConnection();
    emit coreProcess->error(ZJU_ERROR::CLIENT_FAILED);
    coreProcess->complete();

    // The delay the caller is told to wait is the rung the state machine reports,
    // and it is the first one: the timer must not run ahead of the counter the UI
    // shows.
    if (scheduleCalls != 1
        || scheduledDelay != 1000
        || session.state() != ConnectionState::Reconnecting
        || session.reconnectAttempt() != 1
        || session.reconnectAttemptLimit() != 5
        || session.reconnectDelayMs() != scheduledDelay)
    {
        qCritical() << "schedulesTheLadderRungItReports did not schedule the reported rung";
        return false;
    }

    // A disconnect while the retry waits cancels it: no attempt survives a stop.
    session.stop();
    if (session.state() != ConnectionState::Disconnected
        || session.isActive())
    {
        qCritical() << "schedulesTheLadderRungItReports left a pending retry behind";
        return false;
    }
    return true;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    return delegatesProcessLifecycleThroughPort()
        && distinguishesInterruptedConnectionFromStartupFailure()
        && emptySudoPasswordStopsTheSession()
        && sessionExpiryStopsTheCoreAndAsksForALogin()
        && schedulesTheLadderRungItReports()
        && cancelledInteractiveInputSubmitsNewlineBeforeStopping() ? 0 : 1;
}
