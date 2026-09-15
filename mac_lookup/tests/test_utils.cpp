#include "test_utils.h"
#include "../scanner.h"
#include "../oui_database.h"
#include "../thememanager.h"
#include "../utils.h"
#include "../importer.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QHash>
#include <QApplication>
#include <QTemporaryFile>

// ==================== Original Tests ====================

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

// ==================== Scanner Helper Tests ====================

void TestUtils::testCreateLanDevice() {
    NetworkDevice dev = createLanDevice("192.168.1.100", "AA:BB:CC:DD:EE:FF");
    QCOMPARE(dev.type, QString("LAN"));
    QCOMPARE(dev.ip, QString("192.168.1.100"));
    QCOMPARE(dev.mac, QString("AA:BB:CC:DD:EE:FF"));
    QCOMPARE(dev.latency, 0);
    QCOMPARE(dev.signal, 0);
    QCOMPARE(dev.manufacturer, QString(""));
}

void TestUtils::testFormatIpAddress() {
    // 192.168.1.1 = 0xC0A80101
    QCOMPARE(formatIpAddress(0xC0A80101), QString("192.168.1.1"));

    // 10.0.0.1 = 0x0A000001
    QCOMPARE(formatIpAddress(0x0A000001), QString("10.0.0.1"));

    // 255.255.255.255 = 0xFFFFFFFF
    QCOMPARE(formatIpAddress(0xFFFFFFFF), QString("255.255.255.255"));

    // 0.0.0.0
    QCOMPARE(formatIpAddress(0x00000000), QString("0.0.0.0"));
}

void TestUtils::testSortByLatency() {
    QList<NetworkDevice> devices;
    NetworkDevice d1; d1.latency = 200; d1.type = Lan;
    NetworkDevice d2; d2.latency = -1; d2.type = Lan;
    NetworkDevice d3; d3.latency = 50; d3.type = Lan;
    NetworkDevice d4; d4.latency = 100; d4.type = Lan;
    devices << d1 << d2 << d3 << d4;

    sortByLatency(devices);

    // Online devices first, sorted by latency (low to high)
    QCOMPARE(devices[0].latency, 50);
    QCOMPARE(devices[1].latency, 100);
    QCOMPARE(devices[2].latency, 200);
    QCOMPARE(devices[3].latency, -1);  // Offline last
}

void TestUtils::testDeduplicateBySsid() {
    QList<NetworkDevice> devices;
    NetworkDevice d1; d1.ip = "NetworkA"; d1.mac = "AA:BB:CC:DD:EE:01";
    NetworkDevice d2; d2.ip = "NetworkB"; d2.mac = "AA:BB:CC:DD:EE:02";
    NetworkDevice d3; d3.ip = "NetworkA"; d3.mac = "AA:BB:CC:DD:EE:03";  // Duplicate
    NetworkDevice d4; d4.ip = ""; d4.mac = "AA:BB:CC:DD:EE:04";  // No SSID
    NetworkDevice d5; d5.ip = ""; d5.mac = "AA:BB:CC:DD:EE:04";  // Duplicate MAC
    devices << d1 << d2 << d3 << d4 << d5;

    QList<NetworkDevice> result = deduplicateBySsid(devices);

    QCOMPARE(result.size(), 3);  // d1, d2, d4 (d3 and d5 are duplicates)
}

// ==================== OuiDatabase Tests ====================

void TestUtils::testOuiDatabaseLoad() {
    OuiDatabase db;
    QString testFile = QCoreApplication::applicationDirPath() + "/../../data/mac_database.txt";
    db.load(testFile, false);  // Don't use cache for test

    QVERIFY(db.size() > 0);
}

void TestUtils::testOuiDatabaseCache() {
    // Create a temp file with test data
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("AA:BB:CC\tCN\tTest Company\t测试公司\n");
    tempFile.write("00:11:22\tUS\tAnother Company\t另一公司\n");
    tempFile.close();

    QString tempPath = tempFile.fileName();
    QString cachePath = tempPath + ".cache";

    // First load - should parse text file and create cache
    OuiDatabase db1;
    db1.load(tempPath, true);
    QCOMPARE(db1.size(), 2);
    QVERIFY(QFile::exists(cachePath));

    // Second load - should use cache
    OuiDatabase db2;
    db2.load(tempPath, true);
    QCOMPARE(db2.size(), 2);

    // Cleanup
    QFile::remove(cachePath);
}

void TestUtils::testOuiDatabaseLookup() {
    OuiDatabase db;
    QString testFile = QCoreApplication::applicationDirPath() + "/../../data/mac_database.txt";
    db.load(testFile, false);

    // Test that lookup returns non-empty for known prefix
    // (Assuming the database has at least one entry)
    if (db.size() > 0) {
        // Just verify lookup doesn't crash
        QString result = db.lookup("AA:BB:CC:DD:EE:FF");
        Q_UNUSED(result);
    }

    // Test unknown prefix returns empty
    QString unknown = db.lookup("FF:FF:FF:FF:FF:FF");
    QVERIFY(unknown.isEmpty());
}

// ==================== ThemeManager Tests ====================

void TestUtils::testThemeManagerLightSheet() {
    QString sheet = ThemeManager::lightSheet();
    QVERIFY(!sheet.isEmpty());
    QVERIFY(sheet.contains("#scanCard"));
    QVERIFY(sheet.contains("#lanCard"));
    QVERIFY(sheet.contains("#wifiCard"));
}

void TestUtils::testThemeManagerDarkSheet() {
    QString sheet = ThemeManager::darkSheet();
    QVERIFY(!sheet.isEmpty());
    QVERIFY(sheet.contains("#scanCard"));
    QVERIFY(sheet.contains("#lanCard"));
    QVERIFY(sheet.contains("#wifiCard"));
    QVERIFY(sheet.contains("QSpinBox"));  // Dark mode specific
}

void TestUtils::testThemeManagerCardStyle() {
    // Light LAN
    QString lightLan = ThemeManager::cardStyle(true, false);
    QVERIFY(lightLan.contains("#e3f2fd"));

    // Light WiFi
    QString lightWifi = ThemeManager::cardStyle(false, false);
    QVERIFY(lightWifi.contains("#e8f5e9"));

    // Dark LAN
    QString darkLan = ThemeManager::cardStyle(true, true);
    QVERIFY(darkLan.contains("#1a3a4a"));

    // Dark WiFi
    QString darkWifi = ThemeManager::cardStyle(false, true);
    QVERIFY(darkWifi.contains("#1a3a1a"));
}

// ==================== Edge Case Tests ====================

void TestUtils::testFormatMacAddressEdgeCases() {
    // Empty input (0 bytes)
    unsigned char empty[1] = {0x00};
    QCOMPARE(formatMacAddress(empty, 0), QString());

    // Exactly 6 bytes with zeros
    unsigned char zeros[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    QCOMPARE(formatMacAddress(zeros, 6), QString("00:00:00:00:00:00"));

    // Single byte
    unsigned char single[1] = {0xAB};
    QCOMPARE(formatMacAddress(single, 1), QString());

    // 5 bytes (too short)
    unsigned char five[5] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};
    QCOMPARE(formatMacAddress(five, 5), QString());
}

void TestUtils::testFormatIpAddressEdgeCases() {
    // Loopback 127.0.0.1 = 0x7F000001
    QCOMPARE(formatIpAddress(0x7F000001), QString("127.0.0.1"));

    // Link-local 169.254.1.1 = 0xA9FE0101
    QCOMPARE(formatIpAddress(0xA9FE0101), QString("169.254.1.1"));

    // Broadcast 255.255.255.255 = 0xFFFFFFFF
    QCOMPARE(formatIpAddress(0xFFFFFFFF), QString("255.255.255.255"));

    // Single byte values
    QCOMPARE(formatIpAddress(0x01000000), QString("1.0.0.0"));
    QCOMPARE(formatIpAddress(0x00010000), QString("0.1.0.0"));
}

void TestUtils::testSortByLatencyEdgeCases() {
    // Empty list
    QList<NetworkDevice> empty;
    sortByLatency(empty);
    QCOMPARE(empty.size(), 0);

    // All offline
    QList<NetworkDevice> allOffline;
    NetworkDevice o1; o1.latency = -1; o1.type = Lan;
    NetworkDevice o2; o2.latency = -1; o2.type = Lan;
    allOffline << o1 << o2;
    sortByLatency(allOffline);
    QCOMPARE(allOffline[0].latency, -1);
    QCOMPARE(allOffline[1].latency, -1);

    // All same latency
    QList<NetworkDevice> allSame;
    NetworkDevice s1; s1.latency = 100; s1.type = Lan;
    NetworkDevice s2; s2.latency = 100; s2.type = Lan;
    NetworkDevice s3; s3.latency = 100; s3.type = Lan;
    allSame << s1 << s2 << s3;
    sortByLatency(allSame);
    QCOMPARE(allSame[0].latency, 100);
    QCOMPARE(allSame[1].latency, 100);
    QCOMPARE(allSame[2].latency, 100);

    // Mixed: offline should be last
    QList<NetworkDevice> mixed;
    NetworkDevice m1; m1.latency = -1; m1.type = Lan;
    NetworkDevice m2; m2.latency = 50; m2.type = Lan;
    mixed << m1 << m2;
    sortByLatency(mixed);
    QCOMPARE(mixed[0].latency, 50);
    QCOMPARE(mixed[1].latency, -1);
}

void TestUtils::testDeduplicateBySsidEdgeCases() {
    // Empty list
    QList<NetworkDevice> empty;
    QList<NetworkDevice> emptyResult = deduplicateBySsid(empty);
    QCOMPARE(emptyResult.size(), 0);

    // All unique
    QList<NetworkDevice> unique;
    NetworkDevice u1; u1.ip = "Net1"; u1.mac = "AA:AA:AA:AA:AA:01";
    NetworkDevice u2; u2.ip = "Net2"; u2.mac = "AA:AA:AA:AA:AA:02";
    unique << u1 << u2;
    QList<NetworkDevice> uniqueResult = deduplicateBySsid(unique);
    QCOMPARE(uniqueResult.size(), 2);

    // All duplicates (same SSID)
    QList<NetworkDevice> dupes;
    NetworkDevice d1; d1.ip = "SameNet"; d1.mac = "AA:AA:AA:AA:AA:01";
    NetworkDevice d2; d2.ip = "SameNet"; d2.mac = "AA:AA:AA:AA:AA:02";
    dupes << d1 << d2;
    QList<NetworkDevice> dupeResult = deduplicateBySsid(dupes);
    QCOMPARE(dupeResult.size(), 1);
}

void TestUtils::testOuiDatabaseLookupCaseInsensitive() {
    OuiDatabase db;
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("AA:BB:CC\tCN\tTest Company\t测试公司\n");
    tempFile.close();

    db.load(tempFile.fileName(), false);

    // Lowercase MAC should still match
    QString result = db.lookup("aa:bb:cc:dd:ee:ff");
    QCOMPARE(result, QString("Test Company"));

    // Mixed case should still match
    QString result2 = db.lookup("Aa:Bb:Cc:11:22:33");
    QCOMPARE(result2, QString("Test Company"));
}

// ==================== ScanImporter Tests ====================

void TestUtils::testMergeDevicesNewOnly() {
    QList<NetworkDevice> existing;
    QList<NetworkDevice> imported;
    NetworkDevice d1; d1.type = Lan; d1.ip = "192.168.1.1"; d1.mac = "AA:BB:CC:DD:EE:01";
    imported << d1;

    QList<NetworkDevice> merged = ScanImporter::mergeDevices(existing, imported);
    QCOMPARE(merged.size(), 1);
    QCOMPARE(merged[0].mac, QString("AA:BB:CC:DD:EE:01"));
}

void TestUtils::testMergeDevicesUpdateExisting() {
    QList<NetworkDevice> existing;
    NetworkDevice e1; e1.type = Lan; e1.ip = "192.168.1.1"; e1.mac = "AA:BB:CC:DD:EE:01";
    e1.manufacturer = "Old Vendor";
    existing << e1;

    QList<NetworkDevice> imported;
    NetworkDevice d1; d1.type = Lan; d1.ip = "192.168.1.1"; d1.mac = "AA:BB:CC:DD:EE:01";
    d1.manufacturer = "New Vendor";
    imported << d1;

    QList<NetworkDevice> merged = ScanImporter::mergeDevices(existing, imported);
    QCOMPARE(merged.size(), 1);
    QCOMPARE(merged[0].manufacturer, QString("New Vendor"));
}

void TestUtils::testMergeDevicesOverlapping() {
    QList<NetworkDevice> existing;
    NetworkDevice e1; e1.type = Lan; e1.ip = "192.168.1.1"; e1.mac = "AA:BB:CC:DD:EE:01";
    existing << e1;

    QList<NetworkDevice> imported;
    NetworkDevice d1; d1.type = Lan; d1.ip = "192.168.1.2"; d1.mac = "AA:BB:CC:DD:EE:02";
    NetworkDevice d2; d2.type = WiFi; d2.ip = "MyNetwork"; d2.mac = "AA:BB:CC:DD:EE:03";
    imported << d1 << d2;

    QList<NetworkDevice> merged = ScanImporter::mergeDevices(existing, imported);
    QCOMPARE(merged.size(), 3);
}

void TestUtils::testImportCsvWithHeader() {
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write("Type,IP/SSID,MAC,Latency/Signal,Manufacturer\n");
    tempFile.write("LAN,192.168.1.1,AA:BB:CC:DD:EE:01,50 ms,Test Vendor\n");
    tempFile.write("WiFi,MyNetwork,AA:BB:CC:DD:EE:02,-45 dBm,Another Vendor\n");
    tempFile.close();

    QList<NetworkDevice> devices = ScanImporter::importCsv(tempFile.fileName());
    QCOMPARE(devices.size(), 2);
    QCOMPARE(devices[0].type, Lan);
    QCOMPARE(devices[0].ip, QString("192.168.1.1"));
    QCOMPARE(devices[0].latency, 50);
    QCOMPARE(devices[1].type, WiFi);
    QCOMPARE(devices[1].signal, -45);
}

void TestUtils::testImportCsvEmptyFile() {
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.close();

    QList<NetworkDevice> devices = ScanImporter::importCsv(tempFile.fileName());
    QCOMPARE(devices.size(), 0);
}
