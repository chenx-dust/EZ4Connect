#ifndef CONNECTIONUICONTROLLER_H
#define CONNECTIONUICONTROLLER_H

#include <QObject>
#include <QSystemTrayIcon>

#include <functional>

#include "core/connectionerror.h"

class QAction;
class ApplicationLogger;
class AuthDialogCoordinator;
class ConnectionSession;
class QPushButton;
class QSettings;
class SystemProxySession;
class QWidget;

class ConnectionUiController : public QObject
{
    Q_OBJECT

public:
    using SettingsProvider = std::function<QSettings *()>;
    using ProfileIdProvider = std::function<QString()>;
    using NotificationHandler = std::function<void(
        const QString &title,
        const QString &content,
        QSystemTrayIcon::MessageIcon icon
    )>;

    ConnectionUiController(
        QWidget *parentWidget,
        QPushButton *connectButton,
        QPushButton *proxyButton,
        QAction *trayConnectAction,
        ConnectionSession *connectionSession,
        SystemProxySession *systemProxySession,
        AuthDialogCoordinator *authenticationDialogs,
        ApplicationLogger *applicationLogger,
        SettingsProvider settingsProvider,
        ProfileIdProvider profileIdProvider,
        NotificationHandler notificationHandler,
        QObject *parent = nullptr
    );

private:
    void handleConnectClicked();
    void handleProxyClicked();
    void startConnection(
        const QString &username,
        const QString &password,
        const QString &phone = QString()
    );
    void showConnectionError(ZJU_ERROR error);
    QSettings *settings() const;

    QWidget *parentWidget;
    QPushButton *connectButton;
    QPushButton *proxyButton;
    QAction *trayConnectAction;
    ConnectionSession *connectionSession;
    SystemProxySession *systemProxySession;
    AuthDialogCoordinator *authenticationDialogs;
    SettingsProvider settingsProvider;
    ProfileIdProvider profileIdProvider;
    NotificationHandler notificationHandler;
    // One warning per outage: the core reports every failed round, so this is what
    // keeps a single outage from becoming a notification per minute.
    bool keepAliveWarningRaised = false;
};

#endif // CONNECTIONUICONTROLLER_H
