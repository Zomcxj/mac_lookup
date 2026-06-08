// mainwindow.cpp - 统一版（局域网 + WiFi，跨平台）
#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextStream>
#include <QApplication>
#include <QStatusBar>
#include <QFrame>
#include <QClipboard>
#include <QShortcut>
#include <QMenu>
#include <QAction>
#include <QListWidgetItem>
#include <QTimer>
#include <QSet>
#include <QNetworkInterface>
#include <algorithm>

#ifdef Q_OS_WIN
#include <iphlpapi.h>
#include <icmpapi.h>
#include <wlanapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wlanapi.lib")
#pragma comment(lib, "ole32.lib")
#else
#include <QProcess>
#include <QRegularExpression>
#endif

#ifdef Q_OS_WIN
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
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    isScanning = false;
    scanInProgress = false;
    scanTimer = new QTimer(this);
    scanTimer->setSingleShot(false);
    connect(scanTimer, &QTimer::timeout, this, &MainWindow::doScan);
    setupUI();
    applyStyles();

    companyMap.reserve(25000);

    QString defaultPath = QApplication::applicationDirPath() + "/data/mac_database.txt";
    loadData(defaultPath);

    m_deviceMac = getAdapterMac();
    if (!m_deviceMac.isEmpty())
        deviceMacLabel->setText(tr("本机 MAC: ") + m_deviceMac);
}

void MainWindow::setupUI() {
    setWindowTitle(tr("MAC Lookup - LAN + WiFi Scanner"));
    setMinimumSize(520, 500);
    resize(560, 600);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QLabel *titleLabel = new QLabel(tr("设备扫描器"), centralWidget);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);

    QFrame *scanCard = new QFrame(centralWidget);
    scanCard->setObjectName("scanCard");
    QVBoxLayout *scanLayout = new QVBoxLayout(scanCard);
    scanLayout->setContentsMargins(16, 12, 16, 12);
    scanLayout->setSpacing(8);

    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->setSpacing(12);

    scanButton = new QPushButton(tr("🔍 开始扫描"), scanCard);
    scanButton->setObjectName("scanButton");
    scanButton->setCursor(Qt::PointingHandCursor);
    scanButton->setFixedHeight(40);
    btnRow->addWidget(scanButton);

    scanLayout->addLayout(btnRow);

    deviceMacLabel = new QLabel(tr("本机 MAC: 获取中..."), scanCard);
    deviceMacLabel->setObjectName("deviceMacLabel");
    deviceMacLabel->setCursor(Qt::PointingHandCursor);
    deviceMacLabel->setAlignment(Qt::AlignCenter);
    deviceMacLabel->setFixedHeight(24);
    connect(deviceMacLabel, &QLabel::linkActivated, this, &MainWindow::copyDeviceMac);
    scanLayout->addWidget(deviceMacLabel);

    deviceList = new QListWidget(scanCard);
    deviceList->setObjectName("deviceList");
    deviceList->setSelectionMode(QAbstractItemView::SingleSelection);
    deviceList->setContextMenuPolicy(Qt::CustomContextMenu);
    deviceList->setMinimumHeight(120);
    deviceList->setSpacing(4);
    deviceList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scanLayout->addWidget(deviceList, 1);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(28, 16, 28, 28);
    mainLayout->setSpacing(10);
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(scanCard, 1);

    statusLabel = new QLabel(tr("正在加载数据..."));
    statusLabel->setObjectName("statusLabel");
    statusBar()->addWidget(statusLabel, 1);
    statusBar()->setStyleSheet(
        "QStatusBar { background: #e8e8e8; border-top: 1px solid #d0d0d0; min-height: 22px; }"
        "QStatusBar::item { border: none; }"
    );

    connect(scanButton, &QPushButton::clicked, this, &MainWindow::startScan);
    connect(deviceList, &QWidget::customContextMenuRequested, this, &MainWindow::onListContextMenu);

    QShortcut *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, &MainWindow::copySelected);
}

void MainWindow::applyStyles() {
    setStyleSheet(R"(
        QMainWindow { background-color: #f5f5f5; }
        #titleLabel { color: #2d2d2d; font-size: 18px; font-weight: bold; }
        #scanCard { background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 10px; }
        #scanButton {
            background-color: #34a853; color: #ffffff; border: none;
            border-radius: 8px; font-size: 14px; font-weight: bold;
        }
        #scanButton:hover { background-color: #2d9248; }
        #scanButton:pressed { background-color: #267e3e; }
        #deviceList { background-color: transparent; border: none; font-size: 12px; }
        #deviceList::item { padding: 0px; border: none; background: transparent; }
        #deviceMacLabel { color: #666666; font-size: 12px; font-family: 'Consolas', monospace; }
        #deviceMacLabel:hover { color: #4a90d9; }
        #statusLabel { color: #666666; font-size: 12px; }
    )");
}

void MainWindow::loadData(const QString &fileName) {
    QFile file(fileName);
    if (!file.exists()) {
        statusLabel->setText(tr("数据文件未找到，请检查 data/mac_database.txt"));
        return;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        statusLabel->setText(tr("无法加载数据文件"));
        return;
    }

    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif

    int wifiCount = 0;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty()) continue;

        QStringList fields = line.split(QRegularExpression("\\s*\\t\\s*"),
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
            Qt::SkipEmptyParts);
#else
            QString::SkipEmptyParts);
#endif

        int offset = 0;
        if (fields.size() >= 5) {
            offset = 1;
            if (fields[0] == QChar(0x2714))
                wifiCount++;
        } else if (fields.size() < 4) {
            continue;
        }

        QString mac = fields[0 + offset].remove(QRegularExpression("[^0-9A-Fa-f:]")).toUpper();
        if (!mac.isEmpty())
            companyMap.insert(mac, {fields[1 + offset], fields[3 + offset]});
    }
    file.close();

    statusLabel->setText(tr("已加载 %1 条记录 (含 %2 家WiFi厂商)")
                         .arg(companyMap.size()).arg(wifiCount));
}

// ==================== 平台特定：获取本机 MAC ====================

#ifdef Q_OS_WIN
QString MainWindow::getAdapterMac() {
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
#else
QString MainWindow::getAdapterMac() {
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
#endif

// ==================== 平台特定：ARP 扫描 ====================

#ifdef Q_OS_WIN
QList<QString> MainWindow::getSubnetsToScan() {
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

QList<NetworkDevice> MainWindow::arpScan() {
    QHash<QString, NetworkDevice> deviceMap;

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
                    if (macBytes[0] & 0x01) continue;
                    if (macBytes[0] == 0 && macBytes[1] == 0 && macBytes[2] == 0 &&
                        macBytes[3] == 0 && macBytes[4] == 0 && macBytes[5] == 0) continue;

                    QString mac = formatMacAddress(macBytes, 6);

                    quint32 addr = pIpNetTable->table[i].dwAddr;
                    QString ip = QString("%1.%2.%3.%4")
                        .arg(addr & 0xFF).arg((addr >> 8) & 0xFF)
                        .arg((addr >> 16) & 0xFF).arg((addr >> 24) & 0xFF);

                    NetworkDevice dev;
                    dev.type = "LAN";
                    dev.ip = ip;
                    dev.mac = mac;
                    dev.latency = 0;
                    dev.signal = 0;
                    dev.manufacturer = "";
                    deviceMap.insert(mac, dev);
                }
            }
            free(pIpNetTable);
        }
    }

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

                    quint32 addr = pTable2->table[i].dwAddr;
                    QString ip = QString("%1.%2.%3.%4")
                        .arg(addr & 0xFF).arg((addr >> 8) & 0xFF)
                        .arg((addr >> 16) & 0xFF).arg((addr >> 24) & 0xFF);

                    NetworkDevice dev;
                    dev.type = "LAN";
                    dev.ip = ip;
                    dev.mac = mac;
                    dev.latency = 0;
                    dev.signal = 0;
                    dev.manufacturer = "";
                    deviceMap.insert(mac, dev);
                }
            }
            free(pTable2);
        }
    }

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

    std::sort(allDevices.begin(), allDevices.end(), [](const NetworkDevice &a, const NetworkDevice &b) {
        bool aOnline = a.latency >= 0;
        bool bOnline = b.latency >= 0;
        if (aOnline != bOnline) return aOnline;
        if (aOnline && bOnline) return a.latency < b.latency;
        return false;
    });

    return allDevices;
}

#else
// ==================== ARM/Linux: ARP 扫描 ====================

QList<NetworkDevice> MainWindow::arpScan() {
    QList<NetworkDevice> result;
    QHash<QString, NetworkDevice> deviceMap;

    QFile arpFile("/proc/net/arp");
    if (!arpFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;

    QTextStream in(&arpFile);
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

            NetworkDevice dev;
            dev.type = "LAN";
            dev.ip = ip;
            dev.mac = mac;
            dev.latency = 0;
            dev.signal = 0;
            dev.manufacturer = "";
            deviceMap.insert(mac, dev);
        }
    }
    arpFile.close();

    QProcess proc;
    proc.start("ping", QStringList() << "-b" << "-c" << "1" << "-W" << "1"
               << "255.255.255.255");
    proc.waitForFinished(3000);

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

            NetworkDevice dev;
            dev.type = "LAN";
            dev.ip = ip;
            dev.mac = mac;
            dev.latency = 0;
            dev.signal = 0;
            dev.manufacturer = "";
            deviceMap.insert(mac, dev);
        }
    }
    arpFile.close();

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

    std::sort(allDevices.begin(), allDevices.end(), [](const NetworkDevice &a, const NetworkDevice &b) {
        bool aOnline = a.latency >= 0;
        bool bOnline = b.latency >= 0;
        if (aOnline != bOnline) return aOnline;
        if (aOnline && bOnline) return a.latency < b.latency;
        return false;
    });

    return allDevices;
}
#endif

// ==================== 平台特定：WiFi 扫描 ====================

#ifdef Q_OS_WIN
QList<NetworkDevice> MainWindow::wifiScan() {
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
                        ap.type = "WiFi";
                        ap.latency = 0;

                        if (pBssEntry->dot11Ssid.uSSIDLength > 0) {
                            QByteArray raw(reinterpret_cast<const char*>(pBssEntry->dot11Ssid.ucSSID),
                                           pBssEntry->dot11Ssid.uSSIDLength);
                            ap.ip = QString::fromUtf8(raw);
                        }

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

    std::sort(result.begin(), result.end(), [](const NetworkDevice &a, const NetworkDevice &b) {
        return a.signal > b.signal;
    });

    QSet<QString> seenSsid;
    QList<NetworkDevice> deduped;
    for (const NetworkDevice &ap : result) {
        QString key = ap.ip.isEmpty() ? ap.mac : ap.ip;
        if (!seenSsid.contains(key)) {
            seenSsid.insert(key);
            deduped.append(ap);
        }
    }

    return deduped;
}

#else
// ==================== ARM/Linux: WiFi 扫描 ====================

QList<NetworkDevice> MainWindow::wifiScan() {
    QList<NetworkDevice> result;

    QString wifiIface;
    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (iface.name().startsWith("wlan") || iface.name().startsWith("ath")) {
            wifiIface = iface.name();
            break;
        }
    }

    if (wifiIface.isEmpty())
        return result;

    QProcess iwgetidProc;
    iwgetidProc.start("iwgetid", QStringList() << wifiIface << "-r");
    iwgetidProc.waitForFinished(3000);
    QString currentSsid = iwgetidProc.readAllStandardOutput().trimmed();

    if (!currentSsid.isEmpty()) {
        NetworkDevice ap;
        ap.type = "WiFi";
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
            currentDevice.type = "WiFi";
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
                if (currentDevice.signal > 0) {
                    currentDevice.signal = -100 + (currentDevice.signal * 50 / 100);
                }
            }
        }
    }

    if (inCell && !currentDevice.ip.isEmpty()) {
        result.append(currentDevice);
    }

    std::sort(result.begin(), result.end(), [](const NetworkDevice &a, const NetworkDevice &b) {
        return a.signal > b.signal;
    });

    QSet<QString> seenSsid;
    QList<NetworkDevice> deduped;
    for (const NetworkDevice &ap : result) {
        QString key = ap.ip.isEmpty() ? ap.mac : ap.ip;
        if (!seenSsid.contains(key)) {
            seenSsid.insert(key);
            deduped.append(ap);
        }
    }

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
            warnAp.type = "WiFi";
            warnAp.ip = tr("(扫描需要root权限，请使用sudo运行)");
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

// ==================== 通用逻辑 ====================

void MainWindow::startScan() {
    if (!isScanning) {
        isScanning = true;
        discoveredDevices.clear();
        deviceList->clear();
        scanButton->setText(tr("⏹ 停止扫描"));
        scanButton->setStyleSheet("#scanButton { background-color: #ea4335; color: #ffffff; border: none; border-radius: 8px; font-size: 14px; font-weight: bold; }");
        scanTimer->start(5000);
        doScan();
    } else {
        isScanning = false;
        scanTimer->stop();
        scanButton->setText(tr("🔍 开始扫描"));
        scanButton->setStyleSheet("");
        statusLabel->setText(tr("已停止扫描 | 共发现 %1 个设备").arg(discoveredDevices.size()));
    }
}

void MainWindow::doScan() {
    if (scanInProgress) return;
    scanInProgress = true;

    if (isScanning)
        statusLabel->setText(tr("正在扫描..."));

    QtConcurrent::run([this]() {
        QList<NetworkDevice> lanDevices = arpScan();
        QList<NetworkDevice> wifiDevices = wifiScan();

        QList<NetworkDevice> allDevices;
        allDevices.append(lanDevices);
        allDevices.append(wifiDevices);

        for (auto &dev : allDevices)
            lookupManufacturer(dev);

        QTimer::singleShot(0, this, [this, allDevices]() {
            scanInProgress = false;
            onScanFinished(allDevices);
        });
    });
}

void MainWindow::onScanFinished(const QList<NetworkDevice> &devices) {
    // 增量更新：比对已显示的设备，只增删变化项
    QSet<QString> newKeys;
    for (const NetworkDevice &dev : devices) {
        newKeys.insert(dev.type + ":" + (dev.mac.isEmpty() ? dev.ip : dev.mac));
    }

    for (int i = deviceList->count() - 1; i >= 0; i--) {
        QListWidgetItem *item = deviceList->item(i);
        if (!newKeys.contains(item->data(Qt::UserRole).toString())) {
            delete deviceList->takeItem(i);
            discoveredDevices.remove(item->data(Qt::UserRole).toString());
        }
    }

    int lanCount = 0;
    int wifiCount = 0;

    for (const NetworkDevice &dev : devices) {
        QString key = dev.type + ":" + (dev.mac.isEmpty() ? dev.ip : dev.mac);
        if (discoveredDevices.contains(key)) {
            if (dev.type == "LAN") lanCount++;
            else wifiCount++;
            continue;
        }
        discoveredDevices.insert(key);

        if (dev.type == "LAN") lanCount++;
        else wifiCount++;

        QWidget *card = createDeviceCard(dev);
        QListWidgetItem *item = new QListWidgetItem(deviceList);
        item->setData(Qt::UserRole, key);
        item->setSizeHint(card->sizeHint());
        deviceList->setItemWidget(item, card);
    }

    if (isScanning)
        statusLabel->setText(tr("实时扫描中 | 局域网: %1 | WiFi: %2 | 总计: %3")
                            .arg(lanCount).arg(wifiCount).arg(discoveredDevices.size()));
    else
        statusLabel->setText(tr("局域网: %1 | WiFi: %2 | 总计: %3")
                            .arg(lanCount).arg(wifiCount).arg(discoveredDevices.size()));
}

QWidget* MainWindow::createDeviceCard(const NetworkDevice &dev) {
    static const QString lanCardStyle =
        "#deviceCardItem { background: #e3f2fd; border: 1px solid #90caf9; border-radius: 8px; }"
        "#deviceCardItem:hover { border-color: #4a90d9; background: #bbdefb; }";
    static const QString wifiCardStyle =
        "#deviceCardItem { background: #e8f5e9; border: 1px solid #a5d6a7; border-radius: 8px; }"
        "#deviceCardItem:hover { border-color: #34a853; background: #c8e6c9; }";
    static const QString typeLabelStyleTemplate =
        "font-size: 10px; font-weight: bold; color: #ffffff; background: %1; border-radius: 4px; padding: 2px 6px; border: none;";
    static const QString ipLabelStyle =
        "font-size: 14px; font-weight: bold; color: #2d2d2d; border: none;";
    static const QString statusLabelStyleTemplate =
        "font-size: 12px; font-weight: bold; color: %1; border: none;";
    static const QString macLabelStyle =
        "font-size: 12px; color: #666666; font-family: 'Consolas','Courier New',monospace; border: none;";
    static const QString vendorLabelStyle =
        "font-size: 12px; color: #888888; border: none;";

    QFrame *card = new QFrame;
    card->setObjectName("deviceCardItem");

    if (dev.type == "LAN") {
        card->setStyleSheet(lanCardStyle);
    } else {
        card->setStyleSheet(wifiCardStyle);
    }

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(4);

    QHBoxLayout *topRow = new QHBoxLayout;
    topRow->setSpacing(8);

    QLabel *typeLabel = new QLabel(dev.type == "LAN" ? "LAN" : "WiFi");
    typeLabel->setStyleSheet(typeLabelStyleTemplate.arg(dev.type == "LAN" ? "#4a90d9" : "#34a853"));

    QLabel *ipLabel = new QLabel(dev.ip.isEmpty() ? tr("未知") : dev.ip);
    ipLabel->setObjectName("cardSsid");
    ipLabel->setStyleSheet(ipLabelStyle);

    QString statusColor;
    QString statusText;
    if (dev.type == "LAN") {
        if (dev.latency < 0) {
            statusColor = "#999999";
            statusText = tr("离线");
        } else if (dev.latency < 50) {
            statusColor = "#34a853";
            statusText = QString::number(dev.latency) + " ms";
        } else if (dev.latency < 200) {
            statusColor = "#fbbc05";
            statusText = QString::number(dev.latency) + " ms";
        } else {
            statusColor = "#ea4335";
            statusText = QString::number(dev.latency) + " ms";
        }
    } else {
        if (dev.signal >= -50) {
            statusColor = "#34a853";
            statusText = QString::number(dev.signal) + " dBm";
        } else if (dev.signal >= -70) {
            statusColor = "#fbbc05";
            statusText = QString::number(dev.signal) + " dBm";
        } else {
            statusColor = "#ea4335";
            statusText = QString::number(dev.signal) + " dBm";
        }
    }

    QLabel *statusLbl = new QLabel(statusText);
    statusLbl->setObjectName("cardSignal");
    statusLbl->setStyleSheet(statusLabelStyleTemplate.arg(statusColor));

    topRow->addWidget(typeLabel);
    topRow->addWidget(ipLabel, 1);
    topRow->addWidget(statusLbl);
    layout->addLayout(topRow);

    QHBoxLayout *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(16);

    QLabel *macLabel = new QLabel(dev.mac.isEmpty() ? tr("未知 MAC") : dev.mac);
    macLabel->setObjectName("cardMac");
    macLabel->setStyleSheet(macLabelStyle);

    QLabel *vendorLabel = new QLabel(dev.manufacturer.isEmpty() ? tr("未知厂商") : dev.manufacturer);
    vendorLabel->setObjectName("cardVendor");
    vendorLabel->setStyleSheet(vendorLabelStyle);

    bottomRow->addWidget(macLabel);
    bottomRow->addWidget(vendorLabel);
    bottomRow->addStretch();
    layout->addLayout(bottomRow);

    return card;
}

void MainWindow::lookupManufacturer(NetworkDevice &dev) const {
    QString prefix = dev.mac.left(8).toUpper();
    if (companyMap.contains(prefix))
        dev.manufacturer = companyMap[prefix].companyName;
    else
        dev.manufacturer = "未知厂商";
}

void MainWindow::onListContextMenu(const QPoint &pos) {
    QListWidgetItem *item = deviceList->itemAt(pos);
    if (!item) return;
    QWidget *w = deviceList->itemWidget(item);
    if (!w) return;

    QLabel *ipLbl = w->findChild<QLabel*>("cardSsid");
    QLabel *macLbl = w->findChild<QLabel*>("cardMac");
    if (!ipLbl || !macLbl) return;

    QMenu menu(this);
    QAction *copyMac = menu.addAction(tr("复制 MAC 地址"));
    QAction *copyIp = menu.addAction(tr("复制 IP/SSID"));
    QAction *copyAll = menu.addAction(tr("复制全部"));

    QAction *chosen = menu.exec(deviceList->mapToGlobal(pos));
    if (chosen == copyMac) {
        QApplication::clipboard()->setText(macLbl->text());
        statusLabel->setText(tr("已复制 MAC: ") + macLbl->text());
    } else if (chosen == copyIp) {
        QApplication::clipboard()->setText(ipLbl->text());
        statusLabel->setText(tr("已复制: ") + ipLbl->text());
    } else if (chosen == copyAll) {
        QApplication::clipboard()->setText(ipLbl->text() + "\t" + macLbl->text());
        statusLabel->setText(tr("已复制: ") + ipLbl->text());
    }
}

void MainWindow::copySelected() {
    QListWidgetItem *item = deviceList->currentItem();
    if (!item) return;
    QWidget *w = deviceList->itemWidget(item);
    if (!w) return;

    QLabel *ipLbl = w->findChild<QLabel*>("cardSsid");
    QLabel *macLbl = w->findChild<QLabel*>("cardMac");
    if (!ipLbl || !macLbl) return;

    QApplication::clipboard()->setText(ipLbl->text() + "\t" + macLbl->text());
    statusLabel->setText(tr("已复制到剪贴板"));
}

void MainWindow::copyDeviceMac() {
    if (!m_deviceMac.isEmpty()) {
        QApplication::clipboard()->setText(m_deviceMac);
        statusLabel->setText(tr("已复制本机 MAC: ") + m_deviceMac);
    }
}
