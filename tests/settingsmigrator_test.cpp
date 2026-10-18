#include <QCoreApplication>
#include <QDebug>
#include <QSettings>
#include <QTemporaryDir>

#include "application/applicationconstants.h"
#include "application/settingsmigrator.h"

namespace
{
bool migratesKnownVersions()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("profile.ini"), QSettings::IniFormat);
    settings.setValue("Common/ConfigVersion", 4);

    const auto action = SettingsMigrator::prepare(settings);
    const bool passed =
        action == SettingsMigrationAction::None
        && settings.value("ZJUConnect/Protocol").toString() == "easyconnect";
    if (!passed)
    {
        qCritical() << "migratesKnownVersions failed";
    }
    return passed;
}

// A configured target address keeps being probed: that is the only case an old
// settings file says anything about a keep-alive target at all.
bool keepsConfiguredKeepAliveTargetOnUpgrade()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("profile.ini"), QSettings::IniFormat);
    settings.setValue("Common/ConfigVersion", 8);
    settings.setValue("ZJUConnect/KeepAlive", true);
    settings.setValue("ZJUConnect/KeepAliveURL", "http://keepalive.example/");

    const auto action = SettingsMigrator::prepare(settings);
    SettingsMigrator::finish(settings, false);
    const bool passed =
        action == SettingsMigrationAction::None
        && settings.value("ZJUConnect/KeepAliveMode").toString() == "url"
        && !settings.contains("ZJUConnect/KeepAlive")
        && settings.value("Common/ConfigVersion").toInt()
            == ApplicationConstants::ConfigVersion;
    if (!passed)
    {
        qCritical() << "keepsConfiguredKeepAliveTargetOnUpgrade failed"
                    << settings.value("ZJUConnect/KeepAliveMode").toString()
                    << settings.value("Common/ConfigVersion").toInt();
    }
    return passed;
}

// Version 8 turned keep-alive off by default; the migration moves such a file to
// the new default, which leaves the target to the core.
bool enablesKeepAliveForVersion8WithoutTarget()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("profile.ini"), QSettings::IniFormat);
    settings.setValue("Common/ConfigVersion", 8);
    settings.setValue("ZJUConnect/KeepAlive", false);
    settings.setValue("ZJUConnect/KeepAliveURL", "");

    const auto action = SettingsMigrator::prepare(settings);
    SettingsMigrator::finish(settings, false);
    const bool passed =
        action == SettingsMigrationAction::None
        && settings.value("ZJUConnect/KeepAliveMode").toString() == "auto"
        && !settings.contains("ZJUConnect/KeepAlive");
    if (!passed)
    {
        qCritical() << "enablesKeepAliveForVersion8WithoutTarget failed"
                    << settings.value("ZJUConnect/KeepAliveMode").toString();
    }
    return passed;
}

bool recommendsResetForUnsupportedVersions()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("profile.ini"), QSettings::IniFormat);
    settings.setValue("Common/ConfigVersion", 5);
    const bool passed =
        SettingsMigrator::prepare(settings) ==
        SettingsMigrationAction::RecommendReset;
    if (!passed)
    {
        qCritical() << "recommendsResetForUnsupportedVersions failed";
    }
    return passed;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    return migratesKnownVersions()
        && keepsConfiguredKeepAliveTargetOnUpgrade()
        && enablesKeepAliveForVersion8WithoutTarget()
        && recommendsResetForUnsupportedVersions() ? 0 : 1;
}
