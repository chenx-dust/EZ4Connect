#include "terminationwatcher.h"

#include <csignal>

namespace
{
// Set from a signal handler, read from the event loop. `volatile sig_atomic_t` is
// the only type a handler may touch.
volatile std::sig_atomic_t terminationSignal = 0;

void rememberTermination(int)
{
    terminationSignal = 1;
}

// The delay is the worst case between the signal and the shutdown; the program is
// only waiting for its child process to end at that point.
constexpr int pollIntervalMs = 100;
}

TerminationWatcher::TerminationWatcher(QObject *parent)
    : QObject(parent)
{
    std::signal(SIGTERM, rememberTermination);
    std::signal(SIGINT, rememberTermination);

    timer.setInterval(pollIntervalMs);
    connect(&timer, &QTimer::timeout, this, &TerminationWatcher::poll);
    timer.start();
}

TerminationWatcher::~TerminationWatcher()
{
    timer.stop();
    terminationSignal = 0;
    std::signal(SIGTERM, SIG_DFL);
    std::signal(SIGINT, SIG_DFL);
}

void TerminationWatcher::poll()
{
    if (terminationSignal == 0)
    {
        return;
    }

    terminationSignal = 0;
    emit requested();
}
