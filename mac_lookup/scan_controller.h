#ifndef SCAN_CONTROLLER_H
#define SCAN_CONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QSet>
#include <QHash>
#include <QList>
#include <QFuture>
#include <atomic>
#include "scanner.h"

class ScanController : public QObject {
    Q_OBJECT

public:
    explicit ScanController(QObject *parent = nullptr);
    ~ScanController() override;

    void start(int intervalSec);
    void stop();
    bool isScanning() const { return m_isScanning; }

    const QSet<QString> &discoveredDevices() const { return m_discoveredDevices; }
    const QHash<QString, QStringList> &deviceHistory() const { return m_deviceHistory; }

signals:
    void scanFinished(const QList<NetworkDevice> &devices);
    void statusMessage(const QString &msg);
    void animationFrame(const QString &frame);

private slots:
    void doScan();
    void onTimerTick();

private:
    QTimer *m_scanTimer;
    QTimer *m_animTimer;
    int m_animFrame;
    bool m_isScanning;
    bool m_scanInProgress;
    QFuture<void> m_future;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_cancelScan{false};
    QSet<QString> m_discoveredDevices;
    QHash<QString, QStringList> m_deviceHistory;
};

#endif // SCAN_CONTROLLER_H
