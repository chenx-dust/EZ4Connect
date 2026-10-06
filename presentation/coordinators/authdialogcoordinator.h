#ifndef AUTHDIALOGCOORDINATOR_H
#define AUTHDIALOGCOORDINATOR_H

#include <QObject>
#include <QPointer>
#include <memory>

class CredentialStore;

class GraphCaptchaWindow;
class LoginWindow;
class QDialog;
class QSettings;
class SsoLoginWebView;
class SudoWindow;
class QWidget;

class AuthDialogCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit AuthDialogCoordinator(
        QWidget *parentWidget,
        QSettings *settings,
        QObject *parent = nullptr
    );

    void setSettings(QSettings *settings);
    void setCredentialStore(std::shared_ptr<CredentialStore> store);
    void requestLogin(const QString &username, const QString &password);
    void requestPhoneNumber(
        const QString &countryCode,
        const QString &phoneNumber
    );
    void requestSudoPassword();
    void requestGraphCaptcha(const QString &graphFile);
    void requestSmsCode(bool showSkipSecondaryAuthOption);
    void requestTotpCode();
    void requestRadiusCode(bool showSkipSecondaryAuthOption);
    void requestSsoLogin();

signals:
    void loginSubmitted(const QString &username, const QString &password, bool saveDetails);
    void phoneNumberSubmitted(
        const QString &countryCode,
        const QString &phoneNumber,
        bool saveDetails
    );
    void sudoPasswordSubmitted(const QString &password, bool remember);
    void interactiveInputSubmitted(const QByteArray &input);
    void interactiveInputCancelled();

private:
    QWidget *parentWidget;
    QSettings *settings;
    QPointer<LoginWindow> loginWindow;
    QPointer<QDialog> phoneNumberDialog;
    QPointer<SudoWindow> sudoWindow;
    QPointer<GraphCaptchaWindow> graphCaptchaWindow;
    QPointer<SsoLoginWebView> ssoLoginWebView;
    std::shared_ptr<CredentialStore> credentialStore;
};

#endif // AUTHDIALOGCOORDINATOR_H
