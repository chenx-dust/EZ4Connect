#include <QCoreApplication>
#include <QDebug>
#include <QList>

#include "core/sessionstatus.h"

namespace
{
bool parsesSessionStateLines()
{
    // Verbose lines, exactly as the core prints them: the reason is the core's
    // prose and contains spaces, the code sits at the end.
    const SessionStateLine online = parseSessionStateLine(
        "2026/10/18 13:43:01 aTrust session state: online reason=logged in code=0"
    );
    const SessionStateLine restoring = parseSessionStateLine(
        "2026/10/18 13:54:01 aTrust session state: restoring reason=session refresh code=0"
    );
    const SessionStateLine relogin = parseSessionStateLine(
        "aTrust session state: relogin-required reason=session refresh was refused code=75500002"
    );

    if (!online.valid
        || online.state != SessionMaintenanceState::Online
        || online.reason != "logged in"
        || online.code != 0)
    {
        qCritical() << "parsesSessionStateLines failed on the online line" << online.reason;
        return false;
    }
    if (!restoring.valid || restoring.state != SessionMaintenanceState::Restoring)
    {
        qCritical() << "parsesSessionStateLines failed on the restoring line";
        return false;
    }
    if (!relogin.valid
        || relogin.state != SessionMaintenanceState::ReloginRequired
        || relogin.reason != "session refresh was refused"
        || relogin.code != 75500002)
    {
        qCritical() << "parsesSessionStateLines failed on the relogin line" << relogin.reason;
        return false;
    }

    // A word this build does not know is still a valid report: the state is what
    // the GUI cannot act on, not a reason to drop the line.
    const SessionStateLine unknown = parseSessionStateLine(
        "aTrust session state: something-new reason=whatever code=1"
    );
    if (!unknown.valid || unknown.state != SessionMaintenanceState::Unknown)
    {
        qCritical() << "parsesSessionStateLines dropped an unknown state word";
        return false;
    }

    return !parseSessionStateLine("2026/10/18 13:54:01 VPN client started").valid
        && !parseSessionStateLine("aTrust session requires reauthentication (code=0)").valid
        && !parseSessionStateLine("aTrust session state: restoring").valid;
}

bool parsesKeepAliveRounds()
{
    // The current core prints no routed= field: a successful application probe
    // is just the OK line.
    const KeepAliveLine ok = parseKeepAliveLine(
        "KeepAlive: OK status=200 target=http://intranet.example/"
    );
    if (!ok.valid || !ok.ok)
    {
        qCritical() << "parsesKeepAliveRounds failed on the OK round";
        return false;
    }

    // The old line is still accepted; the parser only keys on the prefix.
    const KeepAliveLine routed = parseKeepAliveLine(
        "KeepAlive: OK status=200 target=http://intranet.example/ routed=tunnel"
    );
    if (!routed.valid || !routed.ok)
    {
        qCritical() << "parsesKeepAliveRounds failed on the routed round";
        return false;
    }

    const KeepAliveLine httpFailure = parseKeepAliveLine(
        "KeepAlive: target=http://intranet.example/ status=404"
    );
    const KeepAliveLine transport = parseKeepAliveLine(
        "KeepAlive: target=http://intranet.example/ err=dial tcp: i/o timeout"
    );
    const KeepAliveLine noResolver = parseKeepAliveLine(
        "KeepAlive: No remote resolver available"
    );
    if (!httpFailure.valid || httpFailure.ok
        || !transport.valid || transport.ok
        || !noResolver.valid || noResolver.ok)
    {
        qCritical() << "parsesKeepAliveRounds failed on a failure report";
        return false;
    }

    // Notes about how targets were chosen are not results and must not overwrite
    // the last one.
    return !parseKeepAliveLine("KeepAlive: probing 2 target(s) every 60s").valid
        && !parseKeepAliveLine(
               "KeepAlive: no application target, falling back to the remote-DNS probe"
           ).valid
        && !parseKeepAliveLine("Keep alive is disabled: no application target").valid
        && !parseKeepAliveLine("ordinary output").valid;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    return parsesSessionStateLines() && parsesKeepAliveRounds() ? 0 : 1;
}
