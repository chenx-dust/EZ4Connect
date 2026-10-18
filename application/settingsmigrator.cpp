#include "settingsmigrator.h"

#include <QDebug>
#include <QSettings>

#include "application/applicationconstants.h"
#include "application/defaultsettings.h"
#include "core/connectionprofile.h"

namespace
{
// Version 8 and older stored a "定时保活" switch plus an optional target URL.
// The switch is replaced by ZJUConnect/KeepAliveMode, which defaults to leaving
// the target to the core: an address the user configured keeps being probed,
// anything else gets the new default.
void migrateKeepAliveMode(QSettings &settings)
{
    const QString keepAliveUrl =
        settings.value("ZJUConnect/KeepAliveURL", "").toString().trimmed();
    const KeepAliveMode mode = keepAliveUrl.isEmpty()
        ? KeepAliveMode::Automatic
        : KeepAliveMode::Url;

    settings.setValue("ZJUConnect/KeepAliveMode", keepAliveModeToString(mode));
    settings.remove("ZJUConnect/KeepAlive");
    qInfo().noquote()
        << "保活模式已迁移为" << keepAliveModeToString(mode)
        << (mode == KeepAliveMode::Automatic
                ? "（由核心自行选择目标地址）"
                : "（沿用已配置的目标地址）");
}
}

SettingsMigrationAction SettingsMigrator::prepare(QSettings &settings)
{
    const int configVersion = settings.value("Common/ConfigVersion", -1).toInt();
    if (configVersion == -1)
    {
        DefaultSettings::reset(settings);
        return SettingsMigrationAction::None;
    }
    if (configVersion == 4)
    {
        settings.setValue("ZJUConnect/Protocol", "easyconnect");
        return SettingsMigrationAction::None;
    }
    if (configVersion == 6)
    {
        return SettingsMigrationAction::MigrateAutoStart;
    }
    if (configVersion == 8)
    {
        migrateKeepAliveMode(settings);
        return SettingsMigrationAction::None;
    }
    if (configVersion < ApplicationConstants::ConfigVersion)
    {
        return SettingsMigrationAction::RecommendReset;
    }
    return SettingsMigrationAction::None;
}

void SettingsMigrator::finish(QSettings &settings, bool resetToDefaults)
{
    if (resetToDefaults)
    {
        settings.clear();
        DefaultSettings::reset(settings);
    }
    settings.setValue(
        "Common/ConfigVersion",
        ApplicationConstants::ConfigVersion
    );
    settings.sync();
}
