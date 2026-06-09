#include <QCoreApplication>
#include <QtTest>
#include "test_utils.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    int result = 0;
    TestUtils test;
    result |= QTest::qExec(&test, argc, argv);

    return result;
}
