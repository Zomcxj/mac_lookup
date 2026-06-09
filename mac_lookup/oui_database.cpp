// oui_database.cpp - OUI MAC vendor database loading and lookup
#include "oui_database.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

void OuiDatabase::load(const QString &fileName) {
    m_data.clear();
    m_wifiCount = 0;
    m_data.reserve(25000);

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
}

// Look up manufacturer by OUI prefix (first 3 bytes of MAC, e.g. "AA:BB:CC")
QString OuiDatabase::lookup(const QString &mac) const {
    QString prefix = mac.left(8).toUpper();
    if (m_data.contains(prefix))
        return m_data[prefix].companyName;
    return QString();
}
