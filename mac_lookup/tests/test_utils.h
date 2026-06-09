#ifndef TEST_UTILS_H
#define TEST_UTILS_H

#include <QObject>
#include <QtTest>

class TestUtils : public QObject {
    Q_OBJECT

private slots:
    void testFormatMacAddress();
    void testLoadData();
    void testLookupManufacturer();
};

#endif // TEST_UTILS_H
