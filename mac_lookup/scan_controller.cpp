// scan_controller.cpp - Scan lifecycle management extracted from MainWindow
#include "scan_controller.h"
#include <QtConcurrent>
#include <QTimer>
#include <QDateTime>

namespace {
constexpr int kMaxHistoryEntries = 200;
}

ScanController::ScanController(QObject *parent)
    : QObject(parent)
    , m_animFrame(0)
    , m_isScanning(false)
    , m_scanInProgress(false)
{
    m_scanTimer = new QTimer(this);
    m_scanTimer->setSingleShot(false);
    connect(m_scanTimer, &QTimer::timeout, this, &ScanController::doScan);

    m_animTimer = new QTimer(this);
    m_animTimer->setSingleShot(false);
    connect(m_animTimer, &QTimer::timeout, this, &ScanController::onTimerTick);
}

ScanController::~ScanController() {
    m_stop = true;
    m_cancelScan = true;
    if (m_future.isRunning())
        m_future.waitForFinished();
}

void ScanController::start(int intervalSec) {
    if (m_isScanning) return;

    m_isScanning = true;
    m_discoveredDevices.clear();
    m_animFrame = 0;
    m_animTimer->start(100);
    m_scanTimer->start(intervalSec * 1000);
    doScan();
}

void ScanController::stop() {
    if (!m_isScanning) return;

    m_isScanning = false;
    m_cancelScan = true;
    m_scanTimer->stop();
    m_animTimer->stop();
    emit statusMessage(tr("已停止扫描 | 共发现 %1 个设备").arg(m_discoveredDevices.size()));
}

void ScanController::onTimerTick() {
    QStringList frames = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    m_animFrame = (m_animFrame + 1) % frames.size();
    emit animationFrame(frames[m_animFrame] + " " + tr("扫描中"));
}

void ScanController::doScan() {
    if (!m_isScanning || m_scanInProgress) return;
    m_scanInProgress = true;
    m_cancelScan = false;

    m_future = QtConcurrent::run([this]() {
        QList<NetworkDevice> lanDevices = arpScan(&m_cancelScan);
        QList<NetworkDevice> wifiDevices;
        if (!m_stop.load() && !m_cancelScan.load())
            wifiDevices = wifiScan(&m_cancelScan);

        QList<NetworkDevice> allDevices;
        allDevices.append(lanDevices);
        allDevices.append(wifiDevices);

        QTimer::singleShot(0, this, [this, allDevices]() {
            m_scanInProgress = false;
            if (m_stop.load()) return;

            const bool cancelled = m_cancelScan.load();
            if (!cancelled) {
                // Update device history
                QSet<QString> newKeys;
                for (const NetworkDevice &dev : allDevices) {
                    QString key = dev.type + ":" + (dev.mac.isEmpty() ? dev.ip : dev.mac);
                    newKeys.insert(key);
                }

                // Record devices that went offline
                QStringList keysToRemove;
                for (auto it = m_discoveredDevices.constBegin(); it != m_discoveredDevices.constEnd(); ++it) {
                    if (!newKeys.contains(*it)) {
                        QStringList &history = m_deviceHistory[*it];
                        history.append(
                            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " offline");
                        if (history.size() > kMaxHistoryEntries)
                            history.removeFirst();
                        keysToRemove.append(*it);
                    }
                }
                for (const QString &key : keysToRemove) {
                    m_discoveredDevices.remove(key);
                }

                // Record new devices
                for (const NetworkDevice &dev : allDevices) {
                    QString key = dev.type + ":" + (dev.mac.isEmpty() ? dev.ip : dev.mac);
                    if (!m_discoveredDevices.contains(key)) {
                        m_discoveredDevices.insert(key);
                        QStringList &history = m_deviceHistory[key];
                        history.append(
                            QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " online");
                        if (history.size() > kMaxHistoryEntries)
                            history.removeFirst();
                    }
                }
            }

            emit scanFinished(allDevices);
        });
    });
}
