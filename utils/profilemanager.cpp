#include "profilemanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

ProfileManager::ProfileManager()
{
    configRootPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    profilesPath = QDir(configRootPath).filePath("profiles");
    statePath = QDir(configRootPath).filePath("state.ini");
    defaultProfilePath = QDir(configRootPath).filePath("config.ini");

    ensureStorage();
}

QStringList ProfileManager::listProfiles() const
{
    QDir profilesDir(profilesPath);
    QStringList profiles;
    for (const QFileInfo &entry : profilesDir.entryInfoList(QStringList() << "*.ini", QDir::Files, QDir::Name))
    {
        if (entry.fileName() == "state.ini")
        {
            continue;
        }
        profiles << entry.baseName();
    }
    return profiles;
}

QString ProfileManager::activeProfile() const
{
    QSettings state(statePath, QSettings::IniFormat);
    QString profileId = normalizeProfileId(state.value("Profile/Active").toString());

    if (!QFileInfo::exists(profilePath(profileId)))
    {
        profileId = "";
        if (!QFileInfo::exists(profilePath(profileId)))
        {
            QSettings defaultSettings(profilePath(profileId), QSettings::IniFormat);
            defaultSettings.sync();
        }
        state.setValue("Profile/Active", profileId);
        state.sync();
    }
    return profileId;
}

bool ProfileManager::setActiveProfile(const QString &profileId) const
{
    const QString normalizedId = normalizeProfileId(profileId);
    if (normalizedId.isEmpty() || !QFileInfo::exists(profilePath(normalizedId)))
    {
        return false;
    }

    QSettings state(statePath, QSettings::IniFormat);
    state.setValue("Profile/Active", normalizedId);
    state.sync();
    return true;
}

QString ProfileManager::profilePath(const QString &profileId) const
{
    QString normalizedId = normalizeProfileId(profileId);
    if (normalizedId.isEmpty())
    {
        return defaultProfilePath;
    }
    return QDir(profilesPath).filePath(normalizedId + ".ini");
}

QString ProfileManager::createProfile(const QString &requestedName, const QString &sourcePath) const
{
    const QString normalizedId = ensureUniqueProfileId(normalizeProfileId(requestedName));
    const QString path = profilePath(normalizedId);

    if (!sourcePath.isEmpty() && QFileInfo::exists(sourcePath))
    {
        if (QFile::exists(path))
        {
            QFile::remove(path);
        }
        if (!QFile::copy(sourcePath, path))
        {
            return QString();
        }
    }
    else
    {
        QSettings newSettings(path, QSettings::IniFormat);
        newSettings.sync();
    }

    return normalizedId;
}

bool ProfileManager::renameProfile(const QString &oldId, const QString &newId) const
{
    const QString sourceId = normalizeProfileId(oldId);
    const QString targetId = normalizeProfileId(newId);
    if (sourceId.isEmpty() || targetId.isEmpty() || sourceId == targetId)
    {
        return false;
    }

    const QString sourcePath = profilePath(sourceId);
    const QString targetPath = profilePath(targetId);
    if (!QFileInfo::exists(sourcePath) || QFileInfo::exists(targetPath))
    {
        return false;
    }

    return QFile::rename(sourcePath, targetPath);
}

bool ProfileManager::removeProfile(const QString &profileId) const
{
    const QString normalizedId = normalizeProfileId(profileId);
    if (normalizedId.isEmpty())
    {
        return false;
    }
    return QFile::remove(profilePath(normalizedId));
}

QString ProfileManager::normalizeProfileId(const QString &name) const
{
    QString normalized = name.trimmed();
    normalized.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
    normalized.replace(QRegularExpression("_+"), "_");
    if (normalized.startsWith('_'))
    {
        normalized.remove(0, 1);
    }
    if (normalized.endsWith('_'))
    {
        normalized.chop(1);
    }
    return normalized;
}

bool ProfileManager::autoStartEnabled() const
{
    QSettings state(statePath, QSettings::IniFormat);
    return state.value("Global/AutoStart", false).toBool();
}

void ProfileManager::setAutoStartEnabled(bool enabled) const
{
    QSettings state(statePath, QSettings::IniFormat);
    state.setValue("Global/AutoStart", enabled);
    state.sync();
}

QString ProfileManager::ensureUniqueProfileId(const QString &baseId) const
{
    QString candidate = baseId;
    if (candidate.isEmpty())
    {
        candidate = "profile";
    }

    if (!QFileInfo::exists(profilePath(candidate)))
    {
        return candidate;
    }

    int suffix = 2;
    while (QFileInfo::exists(profilePath(candidate + "_" + QString::number(suffix))))
    {
        suffix++;
    }
    return candidate + "_" + QString::number(suffix);
}

void ProfileManager::ensureStorage() const
{
    QDir configRoot(configRootPath);
    if (!configRoot.exists())
    {
        configRoot.mkpath(".");
    }

    QDir profilesDir(profilesPath);
    if (!profilesDir.exists())
    {
        profilesDir.mkpath(".");
    }
}
