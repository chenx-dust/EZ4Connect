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

    void reportSessionState(const SessionStateLine &line)
    {
        emit sessionStateReported(line);
    }

    void reportKeepAlive(const KeepAliveLine &line)
    {
        emit keepAliveReported(line);
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

bool followsTheCoresSessionReports()
{
    auto *coreProcess = new FakeCoreProcess();
    ConnectionSession session(coreProcess);
    ConnectionProfile profile;
    int statusChanges = 0;
    int reloginCalls = 0;
    QObject::connect(
        &session,
        &ConnectionSession::sessionStatusChanged,
        [&]() { ++statusChanges; }
    );
    QObject::connect(
        &session,
        &ConnectionSession::reloginRequired,
        [&](int) { ++reloginCalls; }
    );

    if (!session.start(profile, {true, 1000}))
    {
        qCritical() << "failed to start session for the session report case";
        return false;
    }
    coreProcess->establishConnection();

    coreProcess->reportSessionState(parseSessionStateLine(
        "aTrust session state: renewing reason=session refresh code=0"
    ));
    if (session.sessionStatus().state != SessionMaintenanceState::Renewing
        || statusChanges != 1)
    {
        qCritical() << "followsTheCoresSessionReports did not follow the renewal";
        return false;
    }

    coreProcess->reportKeepAlive(parseKeepAliveLine(
        "KeepAlive: OK status=200 target=http://intranet.example/"
    ));
    if (!session.sessionStatus().hasKeepAlive
        || !session.sessionStatus().keepAlive.ok
        || !session.sessionStatus().keepAlive.summary.contains("status=200")
        || session.sessionStatus().keepAliveAt.isNull())
    {
        qCritical() << "followsTheCoresSessionReports did not keep the keep-alive result";
        return false;
    }

    // The state line alone is enough: a core may print it without the greppable
    // line, and its reason is what the user sees.
    coreProcess->reportSessionState(parseSessionStateLine(
        "aTrust session state: relogin-required reason=session refresh was refused code=0"
    ));
    if (reloginCalls != 1
        || coreProcess->stopCalls != 1
        || session.state() != ConnectionState::ReloginRequired
        || session.sessionStatus().reason != "session refresh was refused")
    {
        qCritical() << "followsTheCoresSessionReports did not treat the state line as terminal";
        return false;
    }

    // The greppable line that follows it is the same verdict and must not repeat
    // the transition or stop the core twice.
    coreProcess->requireRelogin(0);
    if (reloginCalls != 1 || coreProcess->stopCalls != 1)
    {
        qCritical() << "followsTheCoresSessionReports repeated the terminal transition";
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
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    return delegatesProcessLifecycleThroughPort()
        && distinguishesInterruptedConnectionFromStartupFailure()
        && emptySudoPasswordStopsTheSession()
        && sessionExpiryStopsTheCoreAndAsksForALogin()
        && followsTheCoresSessionReports()
        && cancelledInteractiveInputSubmitsNewlineBeforeStopping() ? 0 : 1;
}
