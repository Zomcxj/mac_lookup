#ifndef SCANNER_H
#define SCANNER_H

#include <QList>
#include <QString>
#include <atomic>

// Device type enumeration
enum DeviceType { Lan, WiFi };

struct NetworkDevice {
    DeviceType type;
    QString ip;          // LAN: IP address, WiFi: SSID
    QString mac;         // MAC address
    int latency;         // LAN: latency ms, WiFi: 0
    int signal;          // WiFi: signal strength dBm, LAN: 0
    QString manufacturer;
};

// Helper functions (used across platforms)
NetworkDevice createLanDevice(const QString &ip, const QString &mac);
QString formatIpAddress(unsigned int addr);
void sortByLatency(QList<NetworkDevice> &devices);
QList<NetworkDevice> deduplicateBySsid(const QList<NetworkDevice> &devices);

// Platform-specific scanning functions.
// cancel: optional flag polled between hosts/batches; returns partial results when set.
QList<NetworkDevice> arpScan(const std::atomic<bool>* cancel = nullptr);
QList<NetworkDevice> wifiScan(const std::atomic<bool>* cancel = nullptr);
QString getAdapterMac();

#ifdef Q_OS_WIN
QList<QString> getSubnetsToScan();
#endif

#endif // SCANNER_H
