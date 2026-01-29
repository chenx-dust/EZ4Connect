#ifndef SETTINGWINDOW_H
#define SETTINGWINDOW_H

#include <QDialog>
#include <QSettings>
#include "ui_settingwindow.h"
#include "extrasettingwindow/extrasettingwindow.h"
#include "authinfowindow/authinfowindow.h"

namespace Ui
{
    class SettingWindow;
}

class SettingWindow : public QDialog
{
Q_OBJECT

public:
    explicit SettingWindow(QWidget *parent = nullptr, QSettings *settings = nullptr);

    ~SettingWindow() override;

private:
    void loadSettings();
    void applySettings();
    bool isAuthSettingChanged();

    Ui::SettingWindow *ui;

    QSettings *settings;

    ExtraSettingWindow *extraSettingWindow;
    AuthInfoWindow *authInfoWindow;

    QString tcpPortForwarding;
    QString udpPortForwarding;
    QString customDNS;
    QString customProxyDomain;
    QString extraArguments;
};

#endif //SETTINGWINDOW_H
