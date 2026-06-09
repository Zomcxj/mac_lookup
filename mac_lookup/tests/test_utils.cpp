#include "test_utils.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QHash>
#include <QApplication>

struct CompanyInfo {
    QString country;
    QString companyName;
};

// Helper: format 6 bytes to MAC string
static QString formatMacAddress(const unsigned char *bytes, int len) {
    if (len < 6) return QString();
    return QString("%1:%2:%3:%4:%5:%6")
        .arg(bytes[0], 2, 16, QChar('0'))
        .arg(bytes[1], 2, 16, QChar('0'))
        .arg(bytes[2], 2, 16, QChar('0'))
        .arg(bytes[3], 2, 16, QChar('0'))
        .arg(bytes[4], 2, 16, QChar('0'))
        .arg(bytes[5], 2, 16, QChar('0'))
        .toUpper();
}

void TestUtils::testFormatMacAddress() {
    unsigned char bytes1[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    QCOMPARE(formatMacAddress(bytes1, 6), QString("AA:BB:CC:DD:EE:FF"));

    unsigned char bytes2[6] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    QCOMPARE(formatMacAddress(bytes2, 6), QString("00:11:22:33:44:55"));

    unsigned char bytes3[6] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    QCOMPARE(formatMacAddress(bytes3, 6), QString("01:02:03:04:05:06"));

    // Too short
    unsigned char bytes4[3] = {0x01, 0x02, 0x03};
    QCOMPARE(formatMacAddress(bytes4, 3), QString());
}

void TestUtils::testLoadData() {
    QHash<QString, CompanyInfo> companyMap;
    companyMap.reserve(25000);

    QString testFile = QCoreApplication::applicationDirPath() + "/../../data/mac_database.txt";
    QFile file(testFile);
    if (!file.exists()) {
        QSKIP("Test data file not found, skipping loadData test");
    }

    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    QTextStream in(&file);
    in.setCodec("UTF-8");

    int wifiCount = 0;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList fields = line.split(QRegularExpression("\\s*\\t\\s*"),
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            Qt::SkipEmptyParts);
#else
            QString::SkipEmptyParts);
#endif

        int offset = 0;
        if (fields.size() >= 5) {
            offset = 1;
            if (fields[0] == QChar(0x2714))
                wifiCount++;
        } else if (fields.size() < 4) {
            continue;
        }

        QString mac = fields[0 + offset].remove(QRegularExpression("[^0-9A-Fa-f:]")).toUpper();
        if (!mac.isEmpty())
            companyMap.insert(mac, {fields[1 + offset], fields[3 + offset]});
    }
    file.close();

    QVERIFY(companyMap.size() > 0);
    QVERIFY(wifiCount > 0);
}

void TestUtils::testLookupManufacturer() {
    QHash<QString, CompanyInfo> companyMap;
    companyMap.insert("AA:BB:CC", {"CN", "Test Company"});
    companyMap.insert("00:11:22", {"US", "Another Company"});

    // Test exact match
    QString mac1 = "AA:BB:CC:DD:EE:FF";
    QString prefix1 = mac1.left(8).toUpper();
    QVERIFY(companyMap.contains(prefix1));
    QCOMPARE(companyMap[prefix1].companyName, QString("Test Company"));

    // Test no match
    QString mac2 = "FF:FF:FF:FF:FF:FF";
    QString prefix2 = mac2.left(8).toUpper();
    QVERIFY(!companyMap.contains(prefix2));
}
