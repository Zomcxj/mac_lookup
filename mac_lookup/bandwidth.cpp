// bandwidth.cpp - Per-device bandwidth monitoring
#include "bandwidth.h"
#include <QProcess>
#include <QNetworkInterface>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>

BandwidthMonitor::BandwidthMonitor(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &BandwidthMonitor::poll);
}

void BandwidthMonitor::start(int intervalMs) {
    m_timer->start(intervalMs);
}

void BandwidthMonitor::stop() {
    m_timer->stop();
}

void BandwidthMonitor::poll() {
#ifdef Q_OS_WIN
    // Windows: Use netstat to get per-connection bytes
    QProcess proc;
    proc.start("netstat", QStringList() << "-e");
    proc.waitForFinished(2000);
    QString output = proc.readAllStandardOutput();

    // Parse netstat output (simplified)
    QStringList lines = output.split('\n');
    for (const QString &line : lines) {
        if (line.contains("bytes")) {
            QStringList parts = line.split(QRegularExpression("\\s+"));
            if (parts.size() >= 3) {
                // This is a simplified approach
                // Real implementation would track per-connection stats
            }
        }
    }
#else
    // Linux: Parse /proc/net/dev for per-interface stats
    readProcNetDev();
#endif

    emit updated(m_info);
}

void BandwidthMonitor::readProcNetDev() {
    QFile file("/proc/net/dev");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QTextStream in(&file);
    // Skip headers
    in.readLine();
    in.readLine();

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        QStringList parts = line.split(QRegularExpression("\\s+:?\\s*"));

        if (parts.size() >= 10) {
            QString iface = parts[0];
            qint64 bytesIn = parts[1].toLongLong();
            qint64 bytesOut = parts[9].toLongLong();

            // Calculate rate
            qint64 rateIn = 0;
            qint64 rateOut = 0;

            if (m_lastBytesIn.contains(iface)) {
                rateIn = bytesIn - m_lastBytesIn[iface];
                rateOut = bytesOut - m_lastBytesOut[iface];
            }

            m_lastBytesIn[iface] = bytesIn;
            m_lastBytesOut[iface] = bytesOut;

            BandwidthInfo info;
            info.ip = iface;
            info.bytesIn = bytesIn;
            info.bytesOut = bytesOut;
            info.rateIn = rateIn;
            info.rateOut = rateOut;

            m_info[iface] = info;
        }
    }
    file.close();
}
