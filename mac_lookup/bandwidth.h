#ifndef BANDWIDTH_H
#define BANDWIDTH_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QTimer>

struct BandwidthInfo {
    QString ip;
    qint64 bytesIn;
    qint64 bytesOut;
    qint64 rateIn;   // bytes/sec
    qint64 rateOut;  // bytes/sec
};

class BandwidthMonitor : public QObject {
    Q_OBJECT

public:
    explicit BandwidthMonitor(QObject *parent = nullptr);

    // Start monitoring
    void start(int intervalMs = 1000);

    // Stop monitoring
    void stop();

    // Get current bandwidth info
    QHash<QString, BandwidthInfo> currentInfo() const { return m_info; }

signals:
    void updated(const QHash<QString, BandwidthInfo> &info);

private slots:
    void poll();

private:
    QTimer *m_timer;
    QHash<QString, BandwidthInfo> m_info;
    QHash<QString, qint64> m_lastBytesIn;
    QHash<QString, qint64> m_lastBytesOut;

    void readProcNetDev();
};

#endif // BANDWIDTH_H
