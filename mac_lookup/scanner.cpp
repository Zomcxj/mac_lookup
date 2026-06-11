// scanner.cpp - Platform-specific network scanning (ARP + WiFi)
#include "scanner.h"
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <algorithm>

// ==================== Helper Functions ====================

NetworkDevice createLanDevice(const QString &ip, const QString &mac) {
    NetworkDevice dev;
    dev.type = Lan;
    dev.ip = ip;
    dev.mac = mac;
    dev.latency = 0;
    dev.signal = 0;
    dev.manufacturer = "";
    return dev;
}

QString formatIpAddress(unsigned int addr) {
    return QString("%1.%2.%3.%4")
        .arg(addr & 0xFF).arg((addr >> 8) & 0xFF)
        .arg((addr >> 16) & 0xFF).arg((addr >> 24) & 0xFF);
}

void sortByLatency(QList<NetworkDevice> &devices) {
    std::sort(devices.begin(), devices.end(), [](const NetworkDevice &a, const NetworkDevice &b) {
        bool aOnline = a.latency >= 0;
        bool bOnline = b.latency >= 0;
        if (aOnline != bOnline) return aOnline;
        if (aOnline && bOnline) return a.latency < b.latency;
        return false;
    });
}

QList<NetworkDevice> deduplicateBySsid(const QList<NetworkDevice> &devices) {
    QSet<QString> seen;
    QList<NetworkDevice> result;
    for (const NetworkDevice &dev : devices) {
        QString key = dev.ip.isEmpty() ? dev.mac : dev.ip;
        if (!seen.contains(key)) {
            seen.insert(key);
            result.append(dev);
        }
    }
    return result;
}

#ifdef Q_OS_WIN
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <wlanapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")
#endif

// Format 6 raw bytes into "XX:XX:XX:XX:XX:XX" uppercase hex string
static QString formatMacAddress(const BYTE *bytes, int len) {
    if (len < 6) return QString();
    return QString("%1:%2:%3:%4:%5:%6")
        .arg(bytes[0], 2, 16, QChar('0'))
        .arg(bytes[1], 2, 16, QChar('0'))
        .arg(bytes[2], 2, 16, QChar('0'))
        .arg(bytes[3], 2, 16, QChar('0'))
        .arg(bytes[4], 2, 16, QChar('0'))
        .arg(bytes[5], 2, 16, QChar('0'))
        .toUpper();
}

// Get adapter MAC address (Windows: prefer wireless, fallback to first non-loopback)
QString getAdapterMac() {
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning))
            continue;
        QString name = iface.name().toLower();
        QString human = iface.humanReadableName().toLower();
        if (name.contains("wireless") || name.contains("wlan") || name.contains("wi-fi") ||
            human.contains("wireless") || human.contains("wlan") || human.contains("wi-fi") ||
            human.contains("无线")) {
            return iface.hardwareAddress().toUpper();
        }
    }
    for (const QNetworkInterface &iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsLoopBack) &&
            (iface.flags() & QNetworkInterface::IsUp) &&
            !iface.hardwareAddress().isEmpty()) {
            return iface.hardwareAddress().toUpper();
        }
    }
    return "";
}

// Get subnets to scan (Windows)
QList<QString> getSubnetsToScan() {
    QList<QString> subnets;
    QSet<QString> seen;

    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning) ||
            (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;
        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                quint32 ipVal = entry.ip().toIPv4Address();
                quint32 maskVal = entry.netmask().toIPv4Address();
                quint32 net = ipVal & maskVal;
                QString subnet = QString("%1.%2.%3.0")
                    .arg((net >> 24) & 0xFF).arg((net >> 16) & 0xFF)
                    .arg((net >> 8) & 0xFF);
                if (!seen.contains(subnet)) {
                    seen.insert(subnet);
                    subnets << subnet;
                }
            }
        }
    }
    return subnets;
}

// ARP scan (Windows):
// 1. Read initial ARP table from Windows API
// 2. Ping broadcast address to trigger ARP replies from new devices
// 3. Re-read ARP table to capture newly discovered devices
// 4. Ping each device individually to measure latency
// 5. Sort results: online devices first, then by latency (lowest first)
QList<NetworkDevice> arpScan() {
    QHash<QString, NetworkDevice> deviceMap;

    // Step 1: Read initial ARP table
    PMIB_IPNETTABLE pIpNetTable = NULL;
    ULONG size = 0;
    DWORD ret = GetIpNetTable(pIpNetTable, &size, FALSE);

    if (ret == ERROR_INSUFFICIENT_BUFFER) {
        pIpNetTable = (PMIB_IPNETTABLE)malloc(size);
        if (pIpNetTable != NULL) {
            if (GetIpNetTable(pIpNetTable, &size, FALSE) == NO_ERROR) {
                for (DWORD i = 0; i < pIpNetTable->dwNumEntries; i++) {
                    BYTE *macBytes = pIpNetTable->table[i].bPhysAddr;
                    UINT macLen = pIpNetTable->table[i].dwPhysAddrLen;
                    if (macLen != 6) continue;
                    if (macBytes[0] & 0x01) continue;  // Skip multicast
                    if (macBytes[0] == 0 && macBytes[1] == 0 && macBytes[2] == 0 &&
                        macBytes[3] == 0 && macBytes[4] == 0 && macBytes[5] == 0) continue;

                    QString mac = formatMacAddress(macBytes, 6);
                    QString ip = formatIpAddress(pIpNetTable->table[i].dwAddr);
                    deviceMap.insert(mac, createLanDevice(ip, mac));
                }
            }
            free(pIpNetTable);
        }
    }

    // Step 2: Ping broadcast to discover more devices
    QList<QString> subnets = getSubnetsToScan();
    for (const QString &subnet : subnets) {
        QString broadcast = subnet.left(subnet.lastIndexOf('.')) + ".255";
        HANDLE hIcmp = IcmpCreateFile();
        if (hIcmp != INVALID_HANDLE_VALUE) {
            ULONG bcastAddr = inet_addr(broadcast.toUtf8().constData());
            char buf[32] = {0};
            char reply[256];
            IcmpSendEcho(hIcmp, bcastAddr, buf, sizeof(buf), NULL, reply, sizeof(reply), 1000);
            IcmpCloseHandle(hIcmp);
        }
    }

    // Step 3: Re-read ARP table for new entries
    PMIB_IPNETTABLE pTable2 = NULL;
    ULONG size2 = 0;
    ret = GetIpNetTable(pTable2, &size2, FALSE);
    if (ret == ERROR_INSUFFICIENT_BUFFER) {
        pTable2 = (PMIB_IPNETTABLE)malloc(size2);
        if (pTable2 != NULL) {
            if (GetIpNetTable(pTable2, &size2, FALSE) == NO_ERROR) {
                for (DWORD i = 0; i < pTable2->dwNumEntries; i++) {
                    BYTE *macBytes = pTable2->table[i].bPhysAddr;
                    UINT macLen = pTable2->table[i].dwPhysAddrLen;
                    if (macLen != 6) continue;
                    if (macBytes[0] & 0x01) continue;
                    if (macBytes[0] == 0 && macBytes[1] == 0 && macBytes[2] == 0 &&
                        macBytes[3] == 0 && macBytes[4] == 0 && macBytes[5] == 0) continue;

                    QString mac = formatMacAddress(macBytes, 6);
                    if (deviceMap.contains(mac)) continue;

                    QString ip = formatIpAddress(pTable2->table[i].dwAddr);
                    deviceMap.insert(mac, createLanDevice(ip, mac));
                }
            }
            free(pTable2);
        }
    }

    // Step 4: Ping each device for latency
    QList<NetworkDevice> allDevices = deviceMap.values();
    for (NetworkDevice &dev : allDevices) {
        HANDLE hIcmp = IcmpCreateFile();
        if (hIcmp != INVALID_HANDLE_VALUE) {
            ULONG ipAddr = inet_addr(dev.ip.toUtf8().constData());
            char sendBuf[32] = {0};
            char replyBuf[sizeof(ICMP_ECHO_REPLY) + 32];
            DWORD pingRet = IcmpSendEcho(hIcmp, ipAddr, sendBuf, sizeof(sendBuf),
                                         NULL, replyBuf, sizeof(replyBuf), 500);
            if (pingRet > 0) {
                PICMP_ECHO_REPLY pEcho = (PICMP_ECHO_REPLY)replyBuf;
                dev.latency = (int)pEcho->RoundTripTime;
            } else {
                dev.latency = -1;
            }
            IcmpCloseHandle(hIcmp);
        }
    }

    sortByLatency(allDevices);
    return allDevices;
}

// WiFi scan (Windows):
// 1. Open WLAN handle and enumerate wireless interfaces
// 2. For each interface: trigger scan, wait for results, parse networks
// 3. For each network: extract SSID, BSSID (MAC), signal quality
// 4. Sort by signal strength (strongest first)
// 5. Deduplicate by SSID (keep strongest signal for each network)
QList<NetworkDevice> wifiScan() {
    QList<NetworkDevice> result;

    HANDLE hClient = NULL;
    DWORD dwCurVersion = 0;
    if (WlanOpenHandle(2, NULL, &dwCurVersion, &hClient) != ERROR_SUCCESS)
        return result;

    PWLAN_INTERFACE_INFO_LIST pIfList = NULL;
    if (WlanEnumInterfaces(hClient, NULL, &pIfList) != ERROR_SUCCESS) {
        WlanCloseHandle(hClient, NULL);
        return result;
    }

    for (DWORD i = 0; i < pIfList->dwNumberOfItems; i++) {
        PWLAN_INTERFACE_INFO pIfInfo = &pIfList->InterfaceInfo[i];

        WlanScan(hClient, &pIfInfo->InterfaceGuid, NULL, NULL, NULL);

        int attempts = 0;
        while (attempts < 10) {
            Sleep(500);
            attempts++;

            PWLAN_AVAILABLE_NETWORK_LIST pBssList = NULL;
            if (WlanGetAvailableNetworkList(hClient, &pIfInfo->InterfaceGuid, 0, NULL, &pBssList) == ERROR_SUCCESS && pBssList) {
                if (pBssList->dwNumberOfItems > 0) {
                    for (DWORD j = 0; j < pBssList->dwNumberOfItems; j++) {
                        PWLAN_AVAILABLE_NETWORK pBssEntry = &pBssList->Network[j];

                        NetworkDevice ap;
                        ap.type = WiFi;
                        ap.latency = 0;

                        if (pBssEntry->dot11Ssid.uSSIDLength > 0) {
                            QByteArray raw(reinterpret_cast<const char*>(pBssEntry->dot11Ssid.ucSSID),
                                           pBssEntry->dot11Ssid.uSSIDLength);
                            ap.ip = QString::fromUtf8(raw);
                        }

                        // Convert WLAN signal quality (0-100) to dBm
                        ap.signal = pBssEntry->wlanSignalQuality == 100 ? -50 :
                                    -100 + pBssEntry->wlanSignalQuality / 2;

                        PWLAN_BSS_LIST pWlanBssList = NULL;
                        if (WlanGetNetworkBssList(hClient, &pIfInfo->InterfaceGuid,
                                                   &pBssEntry->dot11Ssid,
                                                   pBssEntry->dot11BssType,
                                                   pBssEntry->bSecurityEnabled,
                                                   NULL, &pWlanBssList) == ERROR_SUCCESS
                            && pWlanBssList && pWlanBssList->dwNumberOfItems > 0) {
                            DOT11_MAC_ADDRESS &mac = pWlanBssList->wlanBssEntries[0].dot11Bssid;
                            ap.mac = formatMacAddress(mac, 6);
                            WlanFreeMemory(pWlanBssList);
                        }

                        result.append(ap);
                    }
                    WlanFreeMemory(pBssList);
                    break;
                }
                WlanFreeMemory(pBssList);
            }
        }
    }

    if (pIfList) WlanFreeMemory(pIfList);
    WlanCloseHandle(hClient, NULL);

    // Sort by signal strength (strongest first)
    std::sort(result.begin(), result.end(), [](const NetworkDevice &a, const NetworkDevice &b) {
        return a.signal > b.signal;
    });

    return deduplicateBySsid(result);
}

#else
// Linux/ARM implementations

// Get adapter MAC address (Linux: first non-loopback interface)
QString getAdapterMac() {
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp) || !(iface.flags() & QNetworkInterface::IsRunning))
            continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack)
            continue;
        if (!iface.hardwareAddress().isEmpty())
            return iface.hardwareAddress().toUpper();
    }
    return "";
}

// ARP scan (Linux):
// 1. Parse /proc/net/arp to get initial ARP table
// 2. Ping broadcast address to trigger ARP replies
// 3. Re-read ARP table for newly discovered devices
// 4. Ping each device in batches of 16 to measure latency
// 5. Sort results: online devices first, then by latency (lowest first)
QList<NetworkDevice> arpScan() {
    QList<NetworkDevice> result;
    QHash<QString, NetworkDevice> deviceMap;

    // Step 1: Read initial ARP table from /proc/net/arp
    QFile arpFile("/proc/net/arp");
    if (!arpFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;

    QTextStream in(&arpFile);
    if (!in.atEnd()) in.readLine();  // Skip header

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(QRegularExpression("\\s+"),
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            Qt::SkipEmptyParts);
#else
            QString::SkipEmptyParts);
#endif
        if (parts.size() >= 6) {
            QString ip = parts[0];
            QString mac = parts[3].toUpper();

            if (mac == "00:00:00:00:00:00") continue;
            deviceMap.insert(mac, createLanDevice(ip, mac));
        }
    }
    arpFile.close();

    // Step 2: Ping broadcast to discover more devices
    QProcess proc;
    proc.start("ping", QStringList() << "-b" << "-c" << "1" << "-W" << "1"
               << "255.255.255.255");
    proc.waitForFinished(3000);

    // Step 3: Re-read ARP table for new entries
    arpFile.open(QIODevice::ReadOnly | QIODevice::Text);
    in.setDevice(&arpFile);
    if (!in.atEnd()) in.readLine();

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList parts = line.split(QRegularExpression("\\s+"),
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            Qt::SkipEmptyParts);
#else
            QString::SkipEmptyParts);
#endif
        if (parts.size() >= 6) {
            QString ip = parts[0];
            QString mac = parts[3].toUpper();

            if (mac == "00:00:00:00:00:00") continue;
            if (deviceMap.contains(mac)) continue;

            deviceMap.insert(mac, createLanDevice(ip, mac));
        }
    }
    arpFile.close();

    // Step 4: Ping each device for latency (batch of 16)
    QList<NetworkDevice> allDevices = deviceMap.values();
    const int BATCH_SIZE = 16;

    for (int batchStart = 0; batchStart < allDevices.size(); batchStart += BATCH_SIZE) {
        int batchEnd = qMin(batchStart + BATCH_SIZE, allDevices.size());
        QVector<QProcess*> procs;
        QVector<int> indices;

        for (int i = batchStart; i < batchEnd; i++) {
            QProcess *p = new QProcess();
            p->start("ping", QStringList() << "-c" << "1" << "-W" << "1" << allDevices[i].ip);
            procs.append(p);
            indices.append(i);
        }

        for (int j = 0; j < procs.size(); j++) {
            procs[j]->waitForFinished(2000);
            if (procs[j]->exitCode() == 0) {
                QString output = procs[j]->readAllStandardOutput();
                QRegularExpression timeRx("time[=<]([\\d.]+)");
                QRegularExpressionMatch match = timeRx.match(output);
                if (match.hasMatch()) {
                    allDevices[indices[j]].latency = (int)match.captured(1).toDouble();
                }
            } else {
                allDevices[indices[j]].latency = -1;
            }
            delete procs[j];
        }
    }

    sortByLatency(allDevices);
    return allDevices;
}

// WiFi scan (Linux):
// 1. Find WiFi interface (wlan0, ath0, etc.)
// 2. Get current connected network info (iwgetid)
// 3. Run iwlist scan to discover all available networks
// 4. Parse iwlist output to extract SSID, MAC, signal quality
// 5. Convert signal quality to dBm: -100 + (quality * 50 / maxQuality)
// 6. Sort by signal strength, deduplicate by SSID
// 7. If no networks found, check if root permissions are needed
QList<NetworkDevice> wifiScan() {
    QList<NetworkDevice> result;

    // Find WiFi interface
    QString wifiIface;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (iface.name().startsWith("wlan") || iface.name().startsWith("ath")) {
            wifiIface = iface.name();
            break;
        }
    }

    if (wifiIface.isEmpty())
        return result;

    // Get current connected network info
    QProcess iwgetidProc;
    iwgetidProc.start("iwgetid", QStringList() << wifiIface << "-r");
    iwgetidProc.waitForFinished(3000);
    QString currentSsid = iwgetidProc.readAllStandardOutput().trimmed();

    if (!currentSsid.isEmpty()) {
        NetworkDevice ap;
        ap.type = WiFi;
        ap.ip = currentSsid;
        ap.latency = 0;

        QProcess bssidProc;
        bssidProc.start("iwgetid", QStringList() << wifiIface << "-a");
        bssidProc.waitForFinished(3000);
        QString bssidOutput = bssidProc.readAllStandardOutput().trimmed();
        int colonIdx = bssidOutput.lastIndexOf(": ");
        if (colonIdx >= 0) {
            ap.mac = bssidOutput.mid(colonIdx + 2).toUpper().replace("-", ":");
        }

        QProcess signalProc;
        signalProc.start("iwconfig", QStringList() << wifiIface);
        signalProc.waitForFinished(3000);
        QString signalOutput = signalProc.readAllStandardOutput();
        QRegularExpression sigRx("Signal level=(-?\\d+)");
        QRegularExpressionMatch sigMatch = sigRx.match(signalOutput);
        if (sigMatch.hasMatch()) {
            ap.signal = sigMatch.captured(1).toInt();
        }

        result.append(ap);
    }

    // Scan all available networks
    QProcess proc;
    proc.start("iwlist", QStringList() << wifiIface << "scan");
    proc.waitForFinished(10000);

    QString output = proc.readAllStandardOutput();
    QStringList lines = output.split('\n');

    NetworkDevice currentDevice;
    bool inCell = false;

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();

        if (trimmed.startsWith("Cell ")) {
            if (inCell && !currentDevice.ip.isEmpty()) {
                result.append(currentDevice);
            }

            inCell = true;
            currentDevice = NetworkDevice();
            currentDevice.type = WiFi;
            currentDevice.latency = 0;
            currentDevice.signal = 0;

            int macIdx = trimmed.indexOf("Address: ");
            if (macIdx >= 0) {
                currentDevice.mac = trimmed.mid(macIdx + 9).toUpper();
            }
        }
        else if (trimmed.startsWith("ESSID:\"")) {
            currentDevice.ip = trimmed.mid(7, trimmed.length() - 8);
        }
        else if (trimmed.startsWith("Quality=")) {
            int eqIdx = trimmed.indexOf('=');
            int slashIdx = trimmed.indexOf('/');
            if (eqIdx >= 0 && slashIdx >= 0) {
                int quality = trimmed.mid(eqIdx + 1, slashIdx - eqIdx - 1).toInt();
                int maxQuality = trimmed.mid(slashIdx + 1,
                    trimmed.indexOf(' ', slashIdx) - slashIdx - 1).toInt();
                // Convert iwlist Quality (x/max) to dBm: -100 + (quality * 50 / maxQuality)
                if (maxQuality > 0) {
                    currentDevice.signal = -100 + (quality * 50 / maxQuality);
                }
            }
        }
        else if (trimmed.startsWith("Signal level=")) {
            int eqIdx = trimmed.indexOf('=');
            if (eqIdx >= 0) {
                QString val = trimmed.mid(eqIdx + 1).split(' ').first();
                currentDevice.signal = val.toInt();
                // iwconfig reports raw dBm directly if value <= 0
                // If positive, it's a quality percentage, convert to dBm
                if (currentDevice.signal > 0) {
                    currentDevice.signal = -100 + (currentDevice.signal * 50 / 100);
                }
            }
        }
    }

    if (inCell && !currentDevice.ip.isEmpty()) {
        result.append(currentDevice);
    }

    // Sort by signal strength (strongest first)
    std::sort(result.begin(), result.end(), [](const NetworkDevice &a, const NetworkDevice &b) {
        return a.signal > b.signal;
    });

    QList<NetworkDevice> deduped = deduplicateBySsid(result);

    // If no networks found, check if we need root permissions
    if (deduped.isEmpty() && !wifiIface.isEmpty()) {
        QProcess testProc;
        testProc.start("iwlist", QStringList() << wifiIface << "scan");
        testProc.waitForFinished(5000);
        QString errOutput = testProc.readAllStandardError();
        if (errOutput.contains("Operation not permitted") ||
            errOutput.contains("Permission denied") ||
            testProc.exitCode() != 0) {
            result.clear();
            NetworkDevice warnAp;
            warnAp.type = WiFi;
            warnAp.ip = QObject::tr("(扫描需要root权限，请使用sudo运行)");
            warnAp.mac = "";
            warnAp.latency = 0;
            warnAp.signal = 0;
            warnAp.manufacturer = "";
            deduped.append(warnAp);
        }
    }

    return deduped;
}

#endif
