#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QSettings>
#include <QPointer>

#include "application/connectionsession.h"
#include "application/profileservice.h"
#include "application/systemproxysession.h"
#include "presentation/dialogs/settingwindow/settingwindow.h"
#include "presentation/coordinators/authdialogcoordinator.h"
#include "presentation/main/mainwindowcoordinator.h"

namespace Ui
{
    class MainWindow;
}

class ApplicationLogger;
class ApplicationLogFile;
class ConnectionUiController;
class UpdateChecker;

class MainWindow : public QMainWindow
{
Q_OBJECT

public:
    explicit MainWindow(
        ApplicationLogger *logger,
        ApplicationLogFile *logFile,
        QWidget *parent = nullptr
    );

    ~MainWindow() override;

public slots:

    void cleanUpWhenQuit();

protected:
    void closeEvent(QCloseEvent *e) override;

    void changeEvent(QEvent *event) override;

private:
    void upgradeSettings();

    void clearLog();

    void resetZjuConnectUi();

    void applyColorScheme(Qt::ColorScheme scheme);

    void updateConnectionState(ConnectionState state);

    void updateSessionStatus();

    void updateTrayTooltip(const QString &title);

    QString runningDetailText() const;

    QString sessionStatusText() const;

    QString sessionReasonSuffix() const;

    void updateProfileSummary();

    void showNotification(
        const QString &title,
        const QString &content,
        QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::MessageIcon::Information
    );

    void updateVersionInfo();

    void setupTrayIcon();

    void setupProfileMenu();

    void refreshProfileMenu();

    bool switchProfile(const QString &profileId);

    void createProfile();

    void openConfigurationGuide();

    void promptConfigurationGuide(
        const QString &windowTitle,
        const QString &text
    );

    void promptFirstLaunchGuide();

    void renameCurrentProfile();

    void deleteCurrentProfile();

    void gracefullyQuit();

    Ui::MainWindow *ui;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    QMenu *trayProfileMenu;
    QAction *trayConnectAction;
    QAction *trayShowAction;
    QAction *trayCloseAction;
    QAction *newProfileAction;
    QAction *renameProfileAction;
    QAction *deleteProfileAction;
    ConnectionSession *connectionSession = nullptr;
    SystemProxySession *systemProxySession = nullptr;
    AuthDialogCoordinator *authenticationDialogs = nullptr;
    ConnectionUiController *connectionUiController;
    MainWindowCoordinator *coordinator;
    ApplicationLogger *applicationLogger;
    ApplicationLogFile *applicationLogFile;
    UpdateChecker *updateChecker;
    QSettings *settings;
    ProfileService *profileService;
    QString currentProfileId;

    QPointer<SettingWindow> settingWindow;

};

#endif //MAINWINDOW_H
