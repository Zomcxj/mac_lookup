// oui_database.cpp - OUI MAC vendor database loading and lookup
//
// Database format:
//   Text file: MAC_PREFIX\tCOUNTRY\tCOMPANY\tCN_NAME (4 fields)
//   With WiFi flag: ✔\tMAC_PREFIX\tCOUNTRY\tCOMPANY\tCN_NAME (5 fields)
//
// Binary cache: QDataStream file with magic number 0x4F554944 ("OUID")
//   Header: magic, cache version, source size, source mtime, count, wifi count
//   Used to speed up subsequent loads; rebuilt when the source changes.
#include "oui_database.h"
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QSaveFile>
#include <QDataStream>
#include <QTextStream>
#include <QDir>

void OuiDatabase::load(const QString &fileName, bool useCache, const QString &cacheDir) {
    m_data.clear();
    m_wifiCount = 0;
    m_data.reserve(25000);

    QFileInfo sourceInfo(fileName);
    const qint64 sourceSize = sourceInfo.exists() ? sourceInfo.size() : -1;
    const qint64 sourceMtime = sourceInfo.exists()
        ? sourceInfo.lastModified().toMSecsSinceEpoch() : -1;

    // Cache can live outside the install directory (which may be read-only).
    QString cacheFile;
    if (!cacheDir.isEmpty()) {
        QDir().mkpath(cacheDir);
        cacheFile = cacheDir + "/" + sourceInfo.fileName() + ".cache";
    } else {
        cacheFile = fileName + ".cache";
    }

    // Try binary cache first (faster)
    if (useCache && loadCache(cacheFile, sourceSize, sourceMtime))
        return;

    // Parse text file
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // Database format:
        //   With WiFi flag: ✔\tMAC_PREFIX\tCOUNTRY\tCOMPANY\tCN_NAME (5 fields)
        //   Without flag:   MAC_PREFIX\tCOUNTRY\tCOMPANY\tCN_NAME (4 fields)
        QStringList fields = line.split('\t',
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            Qt::SkipEmptyParts);
#else
            QString::SkipEmptyParts);
#endif
        for (QString &field : fields)
            field = field.trimmed();

        int offset = 0;
        if (fields.size() >= 5) {
            offset = 1;
            if (fields[0] == QChar(0x2714))  // ✔ = WiFi-capable vendor
                m_wifiCount++;
        } else if (fields.size() < 4) {
            continue;
        }

        const QString rawMac = fields[0 + offset];
        QString mac;
        mac.reserve(rawMac.size());
        for (const QChar c : rawMac) {
            const ushort u = c.unicode();
            if ((u >= '0' && u <= '9') || (u >= 'a' && u <= 'f') ||
                (u >= 'A' && u <= 'F') || u == ':') {
                mac.append(c.toUpper());
            }
        }

        if (!mac.isEmpty() && fields.size() > 3 + offset)
            m_data.insert(mac, {fields[1 + offset], fields[3 + offset]});
    }
    file.close();

    // Save binary cache for faster subsequent loads
    if (useCache)
        saveCache(cacheFile, sourceSize, sourceMtime);
}

bool OuiDatabase::loadCache(const QString &cacheFile, qint64 sourceSize, qint64 sourceMtime) {
    QFile file(cacheFile);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_5_12);

    quint32 magic = 0;
    quint32 version = 0;
    quint32 count = 0;
    quint32 wifiCount = 0;
    qint64 cachedSize = 0;
    qint64 cachedMtime = 0;

    in >> magic;
    if (magic != 0x4F554944)  // "OUID"
        return false;
    in >> version;
    if (version != 2)
        return false;
    in >> cachedSize >> cachedMtime;
    if (cachedSize != sourceSize || cachedMtime != sourceMtime)
        return false;
    in >> count >> wifiCount;
    if (count > 1000000u)
        return false;

    m_data.clear();
    m_data.reserve(static_cast<int>(count));
    for (quint32 i = 0; i < count; i++) {
        QString key;
        CompanyInfo info;
        in >> key >> info.country >> info.companyName;
        if (in.status() != QDataStream::Ok) {
            m_data.clear();
            m_wifiCount = 0;
            return false;
        }
        m_data.insert(key, info);
    }
    m_wifiCount = static_cast<int>(wifiCount);

    file.close();
    return true;
}

void OuiDatabase::saveCache(const QString &cacheFile, qint64 sourceSize, qint64 sourceMtime) {
    if (sourceSize < 0)
        return;

    QSaveFile file(cacheFile);
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_5_12);

    out << (quint32)0x4F554944;  // "OUID" magic
    out << (quint32)2;           // cache format version
    out << sourceSize << sourceMtime;
    out << (quint32)m_data.size() << (quint32)m_wifiCount;

    auto it = m_data.constBegin();
    while (it != m_data.constEnd()) {
        out << it.key() << it.value().country << it.value().companyName;
        ++it;
    }

    if (out.status() != QDataStream::Ok) {
        file.cancelWriting();
        return;
    }
    file.commit();
}

// Look up manufacturer by OUI prefix (first 3 bytes of MAC, e.g. "AA:BB:CC")
QString OuiDatabase::lookup(const QString &mac) const {
    const auto it = m_data.constFind(mac.left(8).toUpper());
    return it != m_data.constEnd() ? it->companyName : QString();
}
