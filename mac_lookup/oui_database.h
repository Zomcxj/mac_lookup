#ifndef OUI_DATABASE_H
#define OUI_DATABASE_H

#include <QHash>
#include <QString>
#include <QtGlobal>

struct CompanyInfo {
    QString country;
    QString companyName;
};

class OuiDatabase {
public:
    void load(const QString &fileName, bool useCache = true, const QString &cacheDir = QString());
    QString lookup(const QString &mac) const;
    int size() const { return m_data.size(); }
    int wifiVendorCount() const { return m_wifiCount; }

private:
    bool loadCache(const QString &cacheFile, qint64 sourceSize, qint64 sourceMtime);
    void saveCache(const QString &cacheFile, qint64 sourceSize, qint64 sourceMtime);

    QHash<QString, CompanyInfo> m_data;
    int m_wifiCount = 0;
};

#endif // OUI_DATABASE_H
