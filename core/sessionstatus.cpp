#include "sessionstatus.h"

#include <QRegularExpression>

namespace
{
const QString kSessionStatePrefix = QStringLiteral("aTrust session state: ");
const QString kKeepAlivePrefix = QStringLiteral("KeepAlive: ");

// The result shapes the core's keep-alive prints. Anything else with the
// "KeepAlive: " prefix is a note about how targets were chosen ("probing 2
// target(s) every 60s", "no application target, falling back to ..."), which is
// not a result and must not overwrite the last one. The patterns are compiled
// once: this parser sees one line per keep-alive round.
struct KeepAliveShape
{
    QRegularExpression pattern;
    bool ok;
};

const KeepAliveShape kKeepAliveShapes[] = {
    // The core's application-traffic probe: a successful answer, an HTTP
    // failure, a transport failure, or the remote-DNS fallback having no
    // resolver. The old routed= field is not emitted any more.
    {QRegularExpression(QStringLiteral("^OK status=[0-9]+ target=")), true},
    {QRegularExpression(QStringLiteral("^target=.* status=[0-9]+")), false},
    {QRegularExpression(QStringLiteral("^target=.* err=")), false},
    {QRegularExpression(QStringLiteral("^No remote resolver available")), false},
};
}

SessionMaintenanceState sessionMaintenanceStateFromCore(const QString &stateWord)
{
    if (stateWord == QLatin1String("online"))
    {
        return SessionMaintenanceState::Online;
    }
    if (stateWord == QLatin1String("renewing"))
    {
        return SessionMaintenanceState::Renewing;
    }
    if (stateWord == QLatin1String("restoring"))
    {
        return SessionMaintenanceState::Restoring;
    }
    if (stateWord == QLatin1String("relogin-required"))
    {
        return SessionMaintenanceState::ReloginRequired;
    }
    if (stateWord == QLatin1String("offline"))
    {
        return SessionMaintenanceState::Offline;
    }
    return SessionMaintenanceState::Unknown;
}

SessionStateLine parseSessionStateLine(const QString &line)
{
    SessionStateLine result;
    const qsizetype marker = line.indexOf(kSessionStatePrefix);
    if (marker < 0)
    {
        return result;
    }

    // "aTrust session state: restoring reason=session refresh code=0". The reason
    // is the core's prose and contains spaces, so it is everything between
    // "reason=" and the code at the end.
    static const QRegularExpression pattern(
        QStringLiteral("^([a-z-]+) reason=(.*) code=(-?[0-9]+)$")
    );
    const QRegularExpressionMatch match = pattern.match(
        line.mid(marker + kSessionStatePrefix.size()).trimmed()
    );
    if (!match.hasMatch())
    {
        return result;
    }

    result.valid = true;
    result.state = sessionMaintenanceStateFromCore(match.captured(1));
    result.reason = match.captured(2).trimmed();
    result.code = match.captured(3).toInt();
    return result;
}

KeepAliveLine parseKeepAliveLine(const QString &line)
{
    KeepAliveLine result;
    const qsizetype marker = line.indexOf(kKeepAlivePrefix);
    if (marker < 0)
    {
        return result;
    }

    const QString summary = line.mid(marker + kKeepAlivePrefix.size()).trimmed();
    for (const KeepAliveShape &shape : kKeepAliveShapes)
    {
        if (!shape.pattern.match(summary).hasMatch())
        {
            continue;
        }

        result.valid = true;
        result.ok = shape.ok;
        result.summary = summary;
        break;
    }
    return result;
}
