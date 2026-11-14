#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <QObject>
#include <QtTest>

class TestUtils : public QObject {
    Q_OBJECT

private slots:
    // Original tests
    void testFormatMacAddress();
    void testFormatMacAddressEdgeCases();
    void testLoadData();
    void testLookupManufacturer();

    // Scanner helper tests
    void testCreateLanDevice();
    void testFormatIpAddress();
    void testFormatIpAddressEdgeCases();
    void testSortByLatency();
    void testSortByLatencyEdgeCases();
    void testDeduplicateBySsid();
    void testDeduplicateBySsidEdgeCases();

    // OuiDatabase tests
    void testOuiDatabaseLoad();
    void testOuiDatabaseCache();
    void testOuiDatabaseCacheInvalidation();
    void testOuiDatabaseCorruptCacheFallback();
    void testOuiDatabaseCustomCacheDir();
    void testOuiDatabaseLookup();
    void testOuiDatabaseLookupCaseInsensitive();

    // ThemeManager tests
    void testThemeManagerLightSheet();
    void testThemeManagerDarkSheet();
    void testThemeManagerCardStyle();

    // ScanImporter tests
    void testMergeDevicesNewOnly();
    void testMergeDevicesUpdateExisting();
    void testMergeDevicesOverlapping();
    void testImportCsvWithHeader();
    void testImportCsvEmptyFile();
};

#endif // TEST_UTILS_H
