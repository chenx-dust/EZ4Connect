#include <QProcess>

#include "utils.h"

namespace Utils {
    void setDeviceTrust(QObject *parent, const QString &protocol, const QString &server, int port, const QString &profileId, bool trust)
    {
        QStringList args;

        if (!protocol.isEmpty())
        {
            args.append("-protocol");
            args.append(protocol);
        }

        if (!server.isEmpty())
        {
            args.append("-server");
            args.append(server);
        }

        if (port != 0)
        {
            args.append("-port");
            args.append(QString::number(port));
        }

        args.append("-client-data-file");
        args.append(Utils::getClientDataPath(profileId));

        args.append(trust ? "-trust-device" : "-untrust-device");

        QProcess process(parent);
        process.start(Utils::getCorePath(), args);

        if (!process.waitForStarted())
        {
            throw std::runtime_error("核心无法启动");
        }

        if (!process.waitForFinished())
        {
            throw std::runtime_error("核心运行超时");
        }

        QByteArray errorOutput = process.readAllStandardError();
        if ((trust && !errorOutput.contains("Device trusted successfully")) || (!trust && !errorOutput.contains("Device untrusted successfully")))
        {
            throw std::runtime_error(errorOutput);
        }
    }
}
