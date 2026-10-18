#ifndef TERMINATIONWATCHER_H
#define TERMINATIONWATCHER_H

#include <QObject>
#include <QTimer>

// Turns the signals a session or a service manager stops its programs with
// (SIGTERM, SIGINT) into a request to quit, so the shutdown the program already has
// runs instead of the process dying where it stands.
//
// A signal handler may not touch the application, so the handler only remembers
// that the signal arrived and a timer polls that flag from the event loop; the
// caller connects `requested` to the quit it wants.
class TerminationWatcher : public QObject
{
    Q_OBJECT

public:
    explicit TerminationWatcher(QObject *parent = nullptr);
    ~TerminationWatcher() override;

signals:
    void requested();

private:
    void poll();

    QTimer timer;
};

#endif // TERMINATIONWATCHER_H
