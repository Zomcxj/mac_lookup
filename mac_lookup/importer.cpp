// importer.cpp - Scan result import/export and merge
#include "importer.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QObject>

QList<NetworkDevice> ScanImporter::importCsv(const QString &filePath) {
    QList<NetworkDevice> devices;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return devices;

    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif

    bool firstLine = true;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        // Skip header
        if (firstLine) {
            firstLine = false;
            if (line.startsWith("Type,") || line.startsWith("type,"))
                continue;
        }

        NetworkDevice dev = parseCsvLine(line);
        if (!dev.mac.isEmpty() || !dev.ip.isEmpty())
            devices.append(dev);
    }

    file.close();
    return devices;
}

NetworkDevice ScanImporter::parseCsvLine(const QString &line) {
    NetworkDevice dev;
    dev.latency = 0;
    dev.signal = 0;

    // Parse CSV: Type,IP/SSID,MAC,Latency/Signal,Manufacturer
    QStringList fields = line.split(",");
    if (fields.size() < 4) return dev;

    dev.type = fields[0].trimmed() == "LAN" ? Lan : WiFi;
    dev.ip = fields[1].trimmed();
    dev.mac = fields[2].trimmed();

    // Parse latency/signal
    QString metric = fields[3].trimmed();
    if (metric.endsWith("ms")) {
        dev.latency = metric.remove(" ms").toInt();
    } else if (metric.endsWith("dBm")) {
        dev.signal = metric.remove(" dBm").toInt();
    } else if (metric == QObject::tr("离线")) {
        dev.latency = -1;
    }

    if (fields.size() >= 5)
        dev.manufacturer = fields[4].trimmed();

    return dev;
}

QList<NetworkDevice> ScanImporter::mergeDevices(
    const QList<NetworkDevice> &existing,
    const QList<NetworkDevice> &imported)
{
    // Build hash of existing devices by MAC
    QHash<QString, NetworkDevice> deviceMap;
    for (const NetworkDevice &dev : existing) {
        QString key = dev.mac.isEmpty() ? dev.ip : dev.mac;
        deviceMap.insert(key, dev);
    }

    // Merge imported devices
    for (const NetworkDevice &dev : imported) {
        QString key = dev.mac.isEmpty() ? dev.ip : dev.mac;
        if (deviceMap.contains(key)) {
            // Update existing device
            NetworkDevice &existing = deviceMap[key];
            if (!dev.ip.isEmpty()) existing.ip = dev.ip;
            if (!dev.manufacturer.isEmpty()) existing.manufacturer = dev.manufacturer;
            if (dev.latency != 0) existing.latency = dev.latency;
            if (dev.signal != 0) existing.signal = dev.signal;
        } else {
            // Add new device
            deviceMap.insert(key, dev);
        }
    }

    return deviceMap.values();
}

bool ScanImporter::exportCsv(const QString &filePath, const QList<NetworkDevice> &devices) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QTextStream out(&file);
    out << "Type,IP/SSID,MAC,Latency/Signal,Manufacturer\n";

    for (const NetworkDevice &dev : devices) {
        out << (dev.type == Lan ? "LAN" : "WiFi") << ","
            << dev.ip << ","
            << dev.mac << ",";

        if (dev.type == Lan) {
            if (dev.latency < 0)
                out << "离线";
            else
                out << dev.latency << " ms";
        } else {
            out << dev.signal << " dBm";
        }

        out << "," << dev.manufacturer << "\n";
    }

    file.close();
    return true;
}
