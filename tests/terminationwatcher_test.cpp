#include <QCoreApplication>
#include <QDebug>
#include <QTimer>

#include <csignal>

#include "infrastructure/platform/terminationwatcher.h"

namespace
{
// The signal has to reach the normal quit path, not kill the process: this runs the
// real event loop, raises the signal from inside it, and expects the loop to return
// through `aboutToQuit` - which is where the program's own shutdown (clear the
// system proxy, end the core child) hangs off.
bool terminationRunsTheNormalQuit(int signalNumber, const QString &name)
{
    QCoreApplication *application = QCoreApplication::instance();
    TerminationWatcher watcher;
    bool requested = false;
    bool aboutToQuit = false;
    QObject::connect(
        &watcher,
        &TerminationWatcher::requested,
        application,
        [&requested, application]()
        {
            requested = true;
            application->quit();
        }
    );
    QObject::connect(
        application,
        &QCoreApplication::aboutToQuit,
        application,
        [&aboutToQuit]() { aboutToQuit = true; }
    );

    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, application, [application]()
    {
        // A signal that never arrives must fail the test, not hang it.
        application->quit();
    });
    watchdog.start(5000);

    QTimer::singleShot(0, [signalNumber]() { std::raise(signalNumber); });
    application->exec();
    watchdog.stop();

    const bool passed = requested && aboutToQuit;
    if (!passed)
    {
        qCritical() << name << "did not leave through the normal quit path"
                    << "requested=" << requested << "aboutToQuit=" << aboutToQuit;
    }
    return passed;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    return terminationRunsTheNormalQuit(SIGTERM, "SIGTERM")
            && terminationRunsTheNormalQuit(SIGINT, "SIGINT")
        ? 0
        : 1;
}
