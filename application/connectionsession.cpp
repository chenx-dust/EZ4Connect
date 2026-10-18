#include "connectionsession.h"

ConnectionSession::ConnectionSession(CoreProcess *coreProcess, QObject *parent)
    : QObject(parent),
      coreProcess(coreProcess)
{
    coreProcess->setParent(this);
    reconnectTimer.setSingleShot(true);

    connect(coreProcess, &CoreProcess::outputRead, this, &ConnectionSession::outputRead);
    connect(coreProcess, &CoreProcess::graphCaptcha, this, &ConnectionSession::graphCaptcha);
    connect(coreProcess, &CoreProcess::smsCode, this, &ConnectionSession::smsCode);
    connect(coreProcess, &CoreProcess::totpCode, this, &ConnectionSession::totpCode);
    connect(coreProcess, &CoreProcess::randCode, this, &ConnectionSession::randCode);
    connect(coreProcess, &CoreProcess::radiusCode, this, &ConnectionSession::radiusCode);
    connect(coreProcess, &CoreProcess::ssoAuth, this, &ConnectionSession::ssoAuth);
    connect(coreProcess, &CoreProcess::askSudoPass,
            this, &ConnectionSession::handleSudoPasswordRequest);

    connect(coreProcess, &CoreProcess::error, this, [this](ZJU_ERROR error)
    {
        sessionState.recordError(error);
    });
    connect(coreProcess, &CoreProcess::sessionRequiresRelogin,
            this, &ConnectionSession::handleSessionRequiresRelogin);
    connect(coreProcess, &CoreProcess::sessionStateReported,
            this, &ConnectionSession::handleSessionStateReported);
    connect(coreProcess, &CoreProcess::keepAliveReported,
            this, &ConnectionSession::handleKeepAliveReported);
    connect(coreProcess, &CoreProcess::started, this, [this]()
    {
        emit stateChanged(sessionState.state());
    });
    connect(coreProcess, &CoreProcess::connectionEstablished, this, [this]()
    {
        sessionState.connectionEstablished();
        emit stateChanged(sessionState.state());
    });
    connect(coreProcess, &CoreProcess::finished,
            this, &ConnectionSession::handleCoreFinished);
    connect(&reconnectTimer, &QTimer::timeout, this, [this]()
    {
        if (sessionState.state() != ConnectionState::Reconnecting)
        {
            return;
        }
        sessionState.beginReconnect();
        emit stateChanged(sessionState.state());
        startCore();
    });
}

bool ConnectionSession::start(const ConnectionProfile &profile, const ReconnectPolicy &policy)
{
    if (!sessionState.requestStart(policy))
    {
        return false;
    }

    currentProfile = profile;
    emit stateChanged(sessionState.state());
    startCore();
    return true;
}

void ConnectionSession::stop()
{
    const ConnectionState previousState = sessionState.state();
    const bool processNeedsStop = sessionState.requestStop();
    if (previousState == ConnectionState::Reconnecting)
    {
        reconnectTimer.stop();
        emit stateChanged(sessionState.state());
        emit finished(sessionState.error());
        return;
    }

    if (processNeedsStop)
    {
        emit stateChanged(sessionState.state());
        coreProcess->stop();
    }
}

void ConnectionSession::submitInput(const QByteArray &data)
{
    coreProcess->writeInput(data);
}

void ConnectionSession::cancelInteractiveInput()
{
    submitInput("\r\n");
    stop();
}

void ConnectionSession::submitSudoPassword(const QString &password, bool remember)
{
    if (password.isEmpty())
    {
        stop();
        return;
    }

    if (remember)
    {
        savedSudoPassword = password;
    }
    sudoPasswordSubmitted = true;
    submitInput(password.toUtf8() + "\n");
}

ConnectionState ConnectionSession::state() const
{
    return sessionState.state();
}

bool ConnectionSession::isActive() const
{
    return sessionState.isActive();
}

int ConnectionSession::reloginCode() const
{
    return sessionState.reloginCode();
}

const SessionStatus &ConnectionSession::sessionStatus() const
{
    return currentSessionStatus;
}

void ConnectionSession::startCore()
{
    // Every process - the first one and each retry - reports its own session;
    // nothing of the previous one is shown while the new one has not said
    // anything yet. This is the single process-start point, so the reconnect path
    // cannot keep a stale status.
    const bool hadStatus =
        currentSessionStatus.state != SessionMaintenanceState::Unknown
        || currentSessionStatus.hasKeepAlive
        || !currentSessionStatus.reason.isEmpty();
    currentSessionStatus = SessionStatus{};
    if (hadStatus)
    {
        emit sessionStatusChanged();
    }
    sudoPasswordSubmitted = false;
    coreProcess->start(currentProfile);
}

void ConnectionSession::handleCoreFinished()
{
    if (sessionState.processFinished() == ProcessFinishAction::Reconnect)
    {
        emit stateChanged(sessionState.state());
        emit reconnectScheduled(sessionState.reconnectDelayMs());
        reconnectTimer.start(sessionState.reconnectDelayMs());
        return;
    }

    emit stateChanged(sessionState.state());
    emit finished(sessionState.error());
}

void ConnectionSession::handleSessionRequiresRelogin(int code)
{
    beginRelogin(code);
}

void ConnectionSession::handleSessionStateReported(const SessionStateLine &line)
{
    currentSessionStatus.state = line.state;
    currentSessionStatus.reason = line.reason;
    emit sessionStatusChanged();

    // The state line is the core announcing what it learned; the "requires
    // reauthentication" line that follows it is the same verdict in the shape a UI
    // is told to grep, and a core may print either one alone.
    if (line.state == SessionMaintenanceState::ReloginRequired)
    {
        beginRelogin(line.code);
    }
}

void ConnectionSession::handleKeepAliveReported(const KeepAliveLine &line)
{
    currentSessionStatus.hasKeepAlive = true;
    currentSessionStatus.keepAlive = line;
    currentSessionStatus.keepAliveAt = QDateTime::currentDateTime();
    emit sessionStatusChanged();
    emit keepAliveReported(line);
}

void ConnectionSession::beginRelogin(int code)
{
    if (!sessionState.sessionExpired(code))
    {
        return;
    }

    emit stateChanged(sessionState.state());

    // The core keeps running with a session that is already dead, and every
    // request through it fails until a login happens. Stop it - that releases the
    // tunnel and the system proxy - and let the caller offer a fresh login, which
    // starts a new process. Never reconnect from here: on this gateway a restart
    // repairs nothing and only wastes the time the user still has a login dialog
    // to answer.
    coreProcess->stop();

    emit reloginRequired(code);
}

void ConnectionSession::handleSudoPasswordRequest()
{
    if (savedSudoPassword.isEmpty())
    {
        emit askSudoPass();
        return;
    }

    if (sudoPasswordSubmitted)
    {
        savedSudoPassword.clear();
        emit savedSudoPasswordRejected();
        emit askSudoPass();
        return;
    }

    sudoPasswordSubmitted = true;
    submitInput(savedSudoPassword.toUtf8() + "\n");
}
