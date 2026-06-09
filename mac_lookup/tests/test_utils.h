#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <QObject>
#include <QtTest>

class TestUtils : public QObject {
    Q_OBJECT

private slots:
    // Original tests
    void testFormatMacAddress();
    void testLoadData();
    void testLookupManufacturer();

    // Scanner helper tests
    void testCreateLanDevice();
    void testFormatIpAddress();
    void testSortByLatency();
    void testDeduplicateBySsid();

    // OuiDatabase tests
    void testOuiDatabaseLoad();
    void testOuiDatabaseCache();
    void testOuiDatabaseLookup();

    // ThemeManager tests
    void testThemeManagerLightSheet();
    void testThemeManagerDarkSheet();
    void testThemeManagerCardStyle();
};

#endif // TEST_UTILS_H
