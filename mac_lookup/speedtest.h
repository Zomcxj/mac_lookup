#ifndef SPEEDTEST_H
#define SPEEDTEST_H

#include <QObject>
#include <QString>
#include <QElapsedTimer>

class SpeedTest : public QObject {
    Q_OBJECT

public:
    struct Result {
        double downloadMbps;
        double uploadMbps;
        int latencyMs;
        QString server;
    };

    explicit SpeedTest(QObject *parent = nullptr);

    // Start speed test
    void start();

    // Cancel test
    void cancel();

signals:
    void progress(const QString &status);
    void resultReady(const SpeedTest::Result &result);
    void error(const QString &message);

private:
    int measureLatency();
    double measureDownload();
    double measureUpload();
};

#endif // SPEEDTEST_H
