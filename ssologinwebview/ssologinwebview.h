#ifndef SSOLOGINWEBVIEW_H
#define SSOLOGINWEBVIEW_H

#include <QDialog>
#include <QUrl>
#include <QString>
#include <QCloseEvent>

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

    void setInitialUrl(const QUrl &url);
    void setCallbackServerHost(const QString &host);
    QUrl currentUrl() const;

signals:
    void loginCompleted(const QString &url);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupConnections();

    Ui::SsoLoginWebView *ui;
    QString callbackServerHost;
    bool loginCompletedEmitted = false;
};

#endif // SSOLOGINWEBVIEW_H
