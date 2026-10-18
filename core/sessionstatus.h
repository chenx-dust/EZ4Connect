#ifndef SESSIONSTATUS_H
#define SESSIONSTATUS_H

#include <QDateTime>
#include <QString>

// What aTrust session maintenance reports on stdout. The core prints these words
// on purpose so a caller can follow the session without reading prose, and the
// GUI follows them instead of guessing from the data plane: `reason` is the
// core's own text and is shown to the user verbatim.
enum class SessionMaintenanceState
{
    Unknown,
    Online,
    Renewing,
    Restoring,
    ReloginRequired,
    Offline,
};

// One "aTrust session state: <state> reason=<reason> code=<code>" line.
struct SessionStateLine
{
    bool valid = false;
    SessionMaintenanceState state = SessionMaintenanceState::Unknown;
    QString reason;
    int code = 0;
};

// What the core's keep-alive last reported. A line is either a result or a note
// about how targets were chosen; only the former is kept for display.
struct KeepAliveLine
{
    bool valid = false;
    bool ok = false;
    QString summary; // the line without its "KeepAlive: " prefix
};

SessionMaintenanceState sessionMaintenanceStateFromCore(const QString &stateWord);

SessionStateLine parseSessionStateLine(const QString &line);
KeepAliveLine parseKeepAliveLine(const QString &line);

// The status the GUI keeps for display: the core's last words plus when the GUI
// saw them, so a stale status is visibly stale.
struct SessionStatus
{
    SessionMaintenanceState state = SessionMaintenanceState::Unknown;
    QString reason;
    bool hasKeepAlive = false;
    KeepAliveLine keepAlive;
    QDateTime keepAliveAt;
};

#endif // SESSIONSTATUS_H
