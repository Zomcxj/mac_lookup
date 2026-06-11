#ifndef SPEEDTEST_H
#define SPEEDTEST_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QElapsedTimer>

class QNetworkReply;

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
    void start();
    void cancel();

signals:
    void progress(const QString &status);
    void resultReady(const SpeedTest::Result &result);
    void error(const QString &message);

private slots:
    void onDownloadReply(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_mgr;
    QElapsedTimer m_timer;
    int m_latencyMs;
    bool m_cancelled;
};

#endif // SPEEDTEST_H
