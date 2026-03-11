#ifndef PROFILEMANAGER_H
#define PROFILEMANAGER_H

#include <QString>
#include <QStringList>

class ProfileManager
{
public:
    ProfileManager();

    QStringList listProfiles() const;

    QString activeProfile() const;

    bool setActiveProfile(const QString &profileId) const;

    QString profilePath(const QString &profileId) const;

    QString createProfile(const QString &requestedName, const QString &sourcePath = QString()) const;

    bool renameProfile(const QString &oldId, const QString &newId) const;

    bool removeProfile(const QString &profileId) const;

    QString normalizeProfileId(const QString &name) const;

    bool autoStartEnabled() const;

    void setAutoStartEnabled(bool enabled) const;

private:
    QString configRootPath;
    QString profilesPath;
    QString statePath;
    QString defaultProfilePath;

    QString ensureUniqueProfileId(const QString &baseId) const;

    void ensureStorage() const;
};

#endif //PROFILEMANAGER_H
