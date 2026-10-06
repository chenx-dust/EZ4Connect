#ifndef SSOLOGINWEBVIEW_H
#define SSOLOGINWEBVIEW_H

#include <QDialog>
#include <QUrl>
#include <QString>
#include <QCloseEvent>
#include <memory>

class CredentialStore;
class QLabel;
class QPushButton;
class QTimer;

namespace Ui
{
class SsoLoginWebView;
}

class SsoLoginWebView : public QDialog
{
    Q_OBJECT

public:
    explicit SsoLoginWebView(QWidget *parent = nullptr);
    ~SsoLoginWebView() override;

    void setCredentialStore(std::shared_ptr<CredentialStore> store,
                            const QString &configurationFile, const QUrl &vpnServerUrl);
    void setInitialUrl(const QUrl &url);
    void setCallbackServerUrl(const QUrl &url);
    QUrl currentUrl() const;

signals:
    void loginCompleted(const QString &url);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupConnections();
    bool isCallbackUrl(const QUrl &url) const;
    void setupCredentialControls();
    void updateCredentialControls();
    void startCredentialFill();
    void tryCredentialFill();
    void savePageCredentials();
    void forgetPageCredentials();
    QString credentialScope() const;
    QString credentialScript(const QString &operation, const QUrl &url,
                             const QString &username = {}, const QString &password = {}) const;

    Ui::SsoLoginWebView *ui;
    QUrl callbackServerUrl;
    bool loginCompletedEmitted = false;
    std::shared_ptr<CredentialStore> credentialStore;
    QString configurationFile;
    QUrl credentialVpnServerUrl;
    QWidget *credentialControls = nullptr;
    QLabel *credentialStatus = nullptr;
    QPushButton *saveCredentialsButton = nullptr;
    QPushButton *fillCredentialsButton = nullptr;
    QPushButton *forgetCredentialsButton = nullptr;
    QTimer *credentialFillTimer = nullptr;
    int navigationGeneration = 0;
    int fillAttempts = 0;
    bool scriptPending = false;
};

#endif // SSOLOGINWEBVIEW_H
