#ifndef SCANNER_H
#define SCANNER_H

#include <QList>
#include <QString>

struct NetworkDevice {
    QString type;        // "LAN" or "WiFi"
    QString ip;          // LAN: IP address, WiFi: SSID
    QString mac;         // MAC address
    int latency;         // LAN: latency ms, WiFi: 0
    int signal;          // WiFi: signal strength dBm, LAN: 0
    QString manufacturer;
};

// Platform-specific scanning functions
QList<NetworkDevice> arpScan();
QList<NetworkDevice> wifiScan();
QString getAdapterMac();

#ifdef Q_OS_WIN
QList<QString> getSubnetsToScan();
#endif

#endif // SCANNER_H
