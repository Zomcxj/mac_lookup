// oui_database.cpp - OUI MAC vendor database loading and lookup
//
// Database format:
//   Text file: MAC_PREFIX\tCOUNTRY\tCOMPANY\tCN_NAME (4 fields)
//   With WiFi flag: ✔\tMAC_PREFIX\tCOUNTRY\tCOMPANY\tCN_NAME (5 fields)
//
// Binary cache: QDataStream file with magic number 0x4F554944 ("OUID")
//   Used to speed up subsequent loads (avoids text parsing)
#include "oui_database.h"
#include <QFile>
#include <QDataStream>
#include <QTextStream>
#include <QRegularExpression>
#include <QDir>

void OuiDatabase::load(const QString &fileName, bool useCache) {
    m_data.clear();
    m_wifiCount = 0;
    m_data.reserve(25000);

    // Try binary cache first (faster)
    QString cacheFile = fileName + ".cache";
    if (useCache && loadCache(cacheFile))
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
        QStringList fields = line.split(QRegularExpression("\\s*\\t\\s*"),
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            Qt::SkipEmptyParts);
#else
            QString::SkipEmptyParts);
#endif

        int offset = 0;
        if (fields.size() >= 5) {
            offset = 1;
            if (fields[0] == QChar(0x2714))  // ✔ = WiFi-capable vendor
                m_wifiCount++;
        } else if (fields.size() < 4) {
            continue;
        }

        QString mac = fields[0 + offset].remove(QRegularExpression("[^0-9A-Fa-f:]")).toUpper();
        if (!mac.isEmpty())
            m_data.insert(mac, {fields[1 + offset], fields[3 + offset]});
    }
    file.close();

    // Save binary cache for faster subsequent loads
    saveCache(cacheFile);
}

bool OuiDatabase::loadCache(const QString &cacheFile) {
    QFile file(cacheFile);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_5_12);

    quint32 magic;
    in >> magic;
    if (magic != 0x4F554944)  // "OUID"
        return false;

    quint32 count, wifiCount;
    in >> count >> wifiCount;

    for (quint32 i = 0; i < count; i++) {
        QString key;
        CompanyInfo info;
        in >> key >> info.country >> info.companyName;
        m_data.insert(key, info);
    }
    m_wifiCount = (int)wifiCount;

    file.close();
    return true;
}

void OuiDatabase::saveCache(const QString &cacheFile) {
    QFile file(cacheFile);
    if (!file.open(QIODevice::WriteOnly))
        return;

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_5_12);

    out << (quint32)0x4F554944;  // "OUID" magic
    out << (quint32)m_data.size() << (quint32)m_wifiCount;

    auto it = m_data.constBegin();
    while (it != m_data.constEnd()) {
        out << it.key() << it.value().country << it.value().companyName;
        ++it;
    }

    file.close();
}

// Look up manufacturer by OUI prefix (first 3 bytes of MAC, e.g. "AA:BB:CC")
QString OuiDatabase::lookup(const QString &mac) const {
    QString prefix = mac.left(8).toUpper();
    if (m_data.contains(prefix))
        return m_data[prefix].companyName;
    return QString();
}
