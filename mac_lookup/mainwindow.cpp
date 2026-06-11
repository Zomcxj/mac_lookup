// mainwindow.cpp - UI and main logic
#include "mainwindow.h"
#include "thememanager.h"
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
#include <QFileDialog>
#include <QDateTime>
#include <QMessageBox>
#include <QDialog>
#include <QPainter>
#include <QTimer>
#include <QTcpSocket>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    isScanning = false;
    scanInProgress = false;
    darkMode = false;
    dragging = false;
    animFrame = 0;
    scanTimer = new QTimer(this);
    scanTimer->setSingleShot(false);
    connect(scanTimer, &QTimer::timeout, this, &MainWindow::doScan);

    animTimer = new QTimer(this);
    animTimer->setSingleShot(false);
    connect(animTimer, &QTimer::timeout, [this]() {
        QStringList frames = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
        animFrame = (animFrame + 1) % frames.size();
        scanButton->setText(frames[animFrame] + " " + tr("扫描中"));
    });
    setupUI();
    applyStyles();

    QString defaultPath = QApplication::applicationDirPath() + "/data/mac_database.txt";
    ouiDb.load(defaultPath);
    statusLabel->setText(tr("已加载 %1 条记录").arg(ouiDb.size()));

    m_deviceMac = getAdapterMac();
    if (!m_deviceMac.isEmpty())
        deviceMacLabel->setText(tr("本机 MAC: ") + m_deviceMac);
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        QFrame *titleBar = findChild<QFrame*>("titleBar");
        if (titleBar && titleBar->underMouse()) {
            dragging = true;
            dragPos = event->globalPos() - frameGeometry().topLeft();
            event->accept();
        }
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
    if (dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPos() - dragPos);
        event->accept();
    }
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event);
    dragging = false;
}

void MainWindow::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    if (darkMode)
        painter.setBrush(QColor(30, 30, 30));
    else
        painter.setBrush(QColor(245, 245, 245));

    painter.drawRoundedRect(rect(), 12, 12);
}

void MainWindow::setupUI() {
    setWindowFlags(Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle(tr("MAC Lookup - LAN + WiFi Scanner"));
    setMinimumSize(780, 520);
    resize(780, 780);

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setObjectName("centralWidget");
    setCentralWidget(centralWidget);

    // Custom title bar
    QFrame *titleBar = new QFrame(centralWidget);
    titleBar->setObjectName("titleBar");
    titleBar->setFixedHeight(36);
    QHBoxLayout *titleBarLayout = new QHBoxLayout(titleBar);
    titleBarLayout->setContentsMargins(12, 0, 8, 0);
    titleBarLayout->setSpacing(6);

    QLabel *iconLabel = new QLabel(titleBar);
    iconLabel->setObjectName("iconLabel");
    iconLabel->setFixedSize(20, 20);
    QString iconPath = QApplication::applicationDirPath() + "/data/mac_lookup.png";
    if (QFile::exists(iconPath)) {
        QPixmap pix(iconPath);
        iconLabel->setPixmap(pix.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    titleBarLayout->addWidget(iconLabel);

    QLabel *titleText = new QLabel(tr("MAC Lookup - LAN + WiFi Scanner"), titleBar);
    titleText->setObjectName("titleText");
    titleBarLayout->addWidget(titleText);
    titleBarLayout->addStretch();

    QPushButton *minBtn = new QPushButton("—", titleBar);
    minBtn->setObjectName("minBtn");
    minBtn->setFixedSize(30, 24);
    minBtn->setCursor(Qt::PointingHandCursor);
    connect(minBtn, &QPushButton::clicked, this, &MainWindow::showMinimized);
    titleBarLayout->addWidget(minBtn);

    QPushButton *maxBtn = new QPushButton("□", titleBar);
    maxBtn->setObjectName("maxBtn");
    maxBtn->setFixedSize(30, 24);
    maxBtn->setCursor(Qt::PointingHandCursor);
    connect(maxBtn, &QPushButton::clicked, [this, maxBtn]() {
        if (isMaximized()) {
            showNormal();
            maxBtn->setText("□");
        } else {
            showMaximized();
            maxBtn->setText("❐");
        }
    });
    titleBarLayout->addWidget(maxBtn);

    QPushButton *closeBtn = new QPushButton("✕", titleBar);
    closeBtn->setObjectName("closeBtn");
    closeBtn->setFixedSize(30, 24);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &MainWindow::close);
    titleBarLayout->addWidget(closeBtn);

    QLabel *titleLabel = new QLabel(tr("设备扫描器"), centralWidget);
    titleLabel->setObjectName("titleLabel");
    titleLabel->setAlignment(Qt::AlignCenter);

    scanCard = new QFrame(centralWidget);
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

    QLabel *intervalLabel = new QLabel(tr("间隔:"), scanCard);
    intervalLabel->setObjectName("intervalLabel");
    intervalLabel->setStyleSheet("font-size: 12px; color: #666666;");
    intervalLabel->setFixedWidth(35);
    btnRow->addWidget(intervalLabel);

    intervalSpin = new QSpinBox(scanCard);
    intervalSpin->setObjectName("intervalSpin");
    intervalSpin->setRange(1, 60);
    intervalSpin->setValue(5);
    intervalSpin->setSuffix(tr(" 秒"));
    intervalSpin->setFixedHeight(40);
    intervalSpin->setFixedWidth(75);
    intervalSpin->setCursor(Qt::PointingHandCursor);
    btnRow->addWidget(intervalSpin);

    QPushButton *exportButton = new QPushButton(tr("导出"), scanCard);
    exportButton->setObjectName("exportButton");
    exportButton->setCursor(Qt::PointingHandCursor);
    exportButton->setFixedHeight(40);
    exportButton->setFixedWidth(60);
    btnRow->addWidget(exportButton);

    QPushButton *darkModeButton = new QPushButton(tr("暗色"), scanCard);
    darkModeButton->setObjectName("darkModeButton");
    darkModeButton->setCursor(Qt::PointingHandCursor);
    darkModeButton->setFixedHeight(40);
    darkModeButton->setFixedWidth(60);
    btnRow->addWidget(darkModeButton);

    QPushButton *importButton = new QPushButton(tr("导入"), scanCard);
    importButton->setObjectName("exportButton");
    importButton->setCursor(Qt::PointingHandCursor);
    importButton->setFixedHeight(40);
    importButton->setFixedWidth(60);
    btnRow->addWidget(importButton);

    QPushButton *speedButton = new QPushButton(tr("测速"), scanCard);
    speedButton->setObjectName("exportButton");
    speedButton->setCursor(Qt::PointingHandCursor);
    speedButton->setFixedHeight(40);
    speedButton->setFixedWidth(60);
    btnRow->addWidget(speedButton);

    QPushButton *vulnButton = new QPushButton(tr("漏洞"), scanCard);
    vulnButton->setObjectName("exportButton");
    vulnButton->setCursor(Qt::PointingHandCursor);
    vulnButton->setFixedHeight(40);
    vulnButton->setFixedWidth(60);
    btnRow->addWidget(vulnButton);

    scanLayout->addLayout(btnRow);

    filterInput = new QLineEdit(scanCard);
    filterInput->setObjectName("filterInput");
    filterInput->setPlaceholderText(tr("搜索 MAC / IP / 厂商..."));
    filterInput->setClearButtonEnabled(true);
    filterInput->setFixedHeight(32);
    scanLayout->addWidget(filterInput);

    deviceMacLabel = new QLabel(tr("本机 MAC: 获取中..."), scanCard);
    deviceMacLabel->setObjectName("deviceMacLabel");
    deviceMacLabel->setCursor(Qt::PointingHandCursor);
    deviceMacLabel->setAlignment(Qt::AlignCenter);
    deviceMacLabel->setFixedHeight(24);
    connect(deviceMacLabel, &QLabel::linkActivated, this, &MainWindow::copyDeviceMac);
    scanLayout->addWidget(deviceMacLabel);

    lanList = new QListWidget(scanCard);
    lanList->setObjectName("lanList");
    lanList->setSelectionMode(QAbstractItemView::SingleSelection);
    lanList->setContextMenuPolicy(Qt::CustomContextMenu);
    lanList->setMinimumHeight(120);
    lanList->setMinimumWidth(320);
    lanList->setSpacing(4);
    lanList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    lanList->setUniformItemSizes(true);

    wifiList = new QListWidget(scanCard);
    wifiList->setObjectName("wifiList");
    wifiList->setSelectionMode(QAbstractItemView::SingleSelection);
    wifiList->setContextMenuPolicy(Qt::CustomContextMenu);
    wifiList->setMinimumHeight(120);
    wifiList->setMinimumWidth(320);
    wifiList->setSpacing(4);
    wifiList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    wifiList->setUniformItemSizes(true);

    QHBoxLayout *listRow = new QHBoxLayout;
    listRow->setSpacing(8);

    QVBoxLayout *lanCol = new QVBoxLayout;
    QLabel *lanLabel = new QLabel(tr("局域网 (LAN)"), scanCard);
    lanLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #4a90d9;");
    lanCol->addWidget(lanLabel);
    lanCol->addWidget(lanList, 1);

    QVBoxLayout *wifiCol = new QVBoxLayout;
    QLabel *wifiLabel = new QLabel(tr("无线网 (WiFi)"), scanCard);
    wifiLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #34a853;");
    wifiCol->addWidget(wifiLabel);
    wifiCol->addWidget(wifiList, 1);

    listRow->addLayout(lanCol, 1);
    listRow->addLayout(wifiCol, 1);
    scanLayout->addLayout(listRow, 1);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(titleBar);

    QWidget *contentWidget = new QWidget(centralWidget);
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(28, 16, 28, 28);
    contentLayout->setSpacing(10);
    contentLayout->addWidget(titleLabel);
    contentLayout->addWidget(scanCard, 1);
    mainLayout->addWidget(contentWidget, 1);

    statusLabel = new QLabel(tr("正在加载数据..."));
    statusLabel->setObjectName("statusLabel");
    statusBar()->addWidget(statusLabel, 1);

    connect(scanButton, &QPushButton::clicked, this, &MainWindow::startScan);
    connect(filterInput, &QLineEdit::textChanged, this, &MainWindow::filterDevices);
    connect(intervalSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int val) {
        if (isScanning) {
            scanTimer->setInterval(val * 1000);
        }
    });
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportResults);
    connect(darkModeButton, &QPushButton::clicked, this, &MainWindow::toggleDarkMode);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importResults);
    connect(speedButton, &QPushButton::clicked, this, &MainWindow::runSpeedTest);
    connect(vulnButton, &QPushButton::clicked, this, &MainWindow::scanVulnerabilities);
    connect(lanList, &QWidget::customContextMenuRequested, this, &MainWindow::onListContextMenu);
    connect(wifiList, &QWidget::customContextMenuRequested, this, &MainWindow::onListContextMenu);

    QShortcut *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, &MainWindow::copySelected);

    // Style for LAN/WiFi list headers
    lanLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #4a90d9; padding: 2px;");
    wifiLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #34a853; padding: 2px;");
}

void MainWindow::applyStyles() {
    setStyleSheet(ThemeManager::lightSheet());
}

void MainWindow::toggleDarkMode() {
    darkMode = !darkMode;
    setStyleSheet(darkMode ? ThemeManager::darkSheet() : ThemeManager::lightSheet());
    updateAllCardStyles();
}

// ==================== Scan Control ====================

void MainWindow::startScan() {
    if (!isScanning) {
        isScanning = true;
        discoveredDevices.clear();
        lanList->clear();
        wifiList->clear();
        scanButton->setStyleSheet("#scanButton { background-color: #ea4335; color: #ffffff; border: none; border-radius: 8px; font-size: 14px; font-weight: bold; }");
        animFrame = 0;
        animTimer->start(100);
        scanTimer->start(intervalSpin->value() * 1000);
        doScan();
    } else {
        isScanning = false;
        scanTimer->stop();
        animTimer->stop();
        scanButton->setText(tr("🔍 开始扫描"));
        scanButton->setStyleSheet("");
        statusLabel->setText(tr("已停止扫描 | 共发现 %1 个设备").arg(discoveredDevices.size()));
    }
}

void MainWindow::doScan() {
    if (scanInProgress) return;
    scanInProgress = true;

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
    // Incremental update: compare with displayed devices, only add/remove changes
    // Key format: "type:identifier" (e.g. "LAN:AA:BB:CC:DD:EE:FF" or "WiFi:MyNetwork")
    QSet<QString> newKeys;
    for (const NetworkDevice &dev : devices) {
        newKeys.insert(dev.type + ":" + (dev.mac.isEmpty() ? dev.ip : dev.mac));
    }

    auto removeFromList = [&](QListWidget *list) {
        for (int i = list->count() - 1; i >= 0; i--) {
            QListWidgetItem *item = list->item(i);
            if (!newKeys.contains(item->data(Qt::UserRole).toString())) {
                QString key = item->data(Qt::UserRole).toString();
                deviceHistory[key].append(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " offline");
                delete list->takeItem(i);
                discoveredDevices.remove(key);
            }
        }
    };
    removeFromList(lanList);
    removeFromList(wifiList);

    int lanCount = 0;
    int wifiCount = 0;

    for (const NetworkDevice &dev : devices) {
        QString key = dev.type + ":" + (dev.mac.isEmpty() ? dev.ip : dev.mac);
        newKeys.insert(key);

        if (discoveredDevices.contains(key)) {
            if (dev.type == Lan) lanCount++;
            else wifiCount++;
            continue;
        }
        discoveredDevices.insert(key);

        if (dev.type == Lan) lanCount++;
        else wifiCount++;

        deviceHistory[key].append(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " online");

        QWidget *card = createDeviceCard(dev);
        QListWidgetItem *item = new QListWidgetItem();
        item->setData(Qt::UserRole, key);
        item->setSizeHint(card->sizeHint());

        QListWidget *targetList = (dev.type == Lan) ? lanList : wifiList;
        targetList->addItem(item);
        targetList->setItemWidget(item, card);
    }

    if (isScanning)
        statusLabel->setText(tr("实时扫描中 | 局域网: %1 | WiFi: %2 | 总计: %3")
                            .arg(lanCount).arg(wifiCount).arg(discoveredDevices.size()));
    else
        statusLabel->setText(tr("局域网: %1 | WiFi: %2 | 总计: %3")
                            .arg(lanCount).arg(wifiCount).arg(discoveredDevices.size()));
}

// ==================== Device Card ====================

static void styleCard(QFrame *card, QLabel *typeLbl, QLabel *ssidLbl, QLabel *macLbl, QLabel *vendorLbl, bool isDark, bool isLan) {
    card->setStyleSheet(ThemeManager::cardStyle(isLan, isDark));
    typeLbl->setStyleSheet(ThemeManager::typeLabelStyle(isLan, isDark));
    ssidLbl->setStyleSheet(ThemeManager::ssidStyle(isDark));
    macLbl->setStyleSheet(ThemeManager::macStyle(isDark));
    vendorLbl->setStyleSheet(ThemeManager::vendorStyle(isDark));
}

void MainWindow::updateAllCardStyles() {
    for (QListWidget *list : {lanList, wifiList}) {
        for (int i = 0; i < list->count(); i++) {
            QWidget *w = list->itemWidget(list->item(i));
            if (!w) continue;
            QFrame *card = qobject_cast<QFrame*>(w);
            QLabel *typeLbl = w->findChild<QLabel*>("lanTypeLabel");
            if (!typeLbl) typeLbl = w->findChild<QLabel*>("wifiTypeLabel");
            QLabel *ssidLbl = w->findChild<QLabel*>("cardSsid");
            QLabel *macLbl = w->findChild<QLabel*>("cardMac");
            QLabel *vendorLbl = w->findChild<QLabel*>("cardVendor");
            QLabel *signalLbl = w->findChild<QLabel*>("cardSignal");
            if (card && typeLbl && ssidLbl && macLbl && vendorLbl) {
                styleCard(card, typeLbl, ssidLbl, macLbl, vendorLbl, darkMode, card->objectName() == "lanCard");
            }
            if (signalLbl) {
                QString sc = signalLbl->styleSheet();
                QString c = sc.contains("#34a853") ? "#34a853" : sc.contains("#fbbc05") ? "#fbbc05" : sc.contains("#ea4335") ? "#ea4335" : "#999999";
                signalLbl->setStyleSheet(ThemeManager::signalStyle(c));
            }
        }
    }
}

QWidget* MainWindow::createDeviceCard(const NetworkDevice &dev) {
    QFrame *card = new QFrame;
    card->setObjectName(dev.type == Lan ? "lanCard" : "wifiCard");
    card->setMinimumHeight(60);

    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(4);

    QHBoxLayout *topRow = new QHBoxLayout;
    topRow->setSpacing(8);

    QLabel *typeLabel = new QLabel(dev.type == Lan ? "LAN" : "WiFi");
    typeLabel->setObjectName(dev.type == Lan ? "lanTypeLabel" : "wifiTypeLabel");

    QLabel *ipLabel = new QLabel(dev.ip.isEmpty() ? tr("未知") : dev.ip);
    ipLabel->setObjectName("cardSsid");

    QString statusColor;
    QString statusText;
    if (dev.type == Lan) {
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
    statusLbl->setStyleSheet(QString("font-size:12px;font-weight:bold;color:%1;border:none;").arg(statusColor));

    topRow->addWidget(typeLabel);
    topRow->addWidget(ipLabel, 1);
    topRow->addWidget(statusLbl);
    layout->addLayout(topRow);

    QHBoxLayout *bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(16);

    QLabel *macLabel = new QLabel(dev.mac.isEmpty() ? tr("未知 MAC") : dev.mac);
    macLabel->setObjectName("cardMac");

    QLabel *vendorLabel = new QLabel(dev.manufacturer.isEmpty() ? tr("未知厂商") : dev.manufacturer);
    vendorLabel->setObjectName("cardVendor");

    bottomRow->addWidget(macLabel);
    bottomRow->addWidget(vendorLabel);
    bottomRow->addStretch();
    layout->addLayout(bottomRow);

    styleCard(card, typeLabel, ipLabel, macLabel, vendorLabel, darkMode, dev.type == Lan);
    return card;
}

// Look up manufacturer by OUI prefix (first 3 bytes of MAC, e.g. "AA:BB:CC")
void MainWindow::lookupManufacturer(NetworkDevice &dev) const {
    QString result = ouiDb.lookup(dev.mac);
    dev.manufacturer = result.isEmpty() ? tr("未知厂商") : result;
}

// ==================== Context Menu & Copy ====================

void MainWindow::onListContextMenu(const QPoint &pos) {
    QListWidget *senderList = qobject_cast<QListWidget*>(sender());
    if (!senderList) return;

    QListWidgetItem *item = senderList->itemAt(pos);
    if (!item) return;
    QWidget *w = senderList->itemWidget(item);
    if (!w) return;

    QLabel *ipLbl = w->findChild<QLabel*>("cardSsid");
    QLabel *macLbl = w->findChild<QLabel*>("cardMac");
    if (!ipLbl || !macLbl) return;

    QMenu menu(this);
    QAction *copyMac = menu.addAction(tr("复制 MAC 地址"));
    QAction *copyIp = menu.addAction(tr("复制 IP/SSID"));
    QAction *copyAll = menu.addAction(tr("复制全部"));
    menu.addSeparator();
    QAction *portScan = menu.addAction(tr("端口扫描"));
    QAction *pingAct = menu.addAction(tr("Ping"));
    QAction *traceAct = menu.addAction(tr("Traceroute"));
    QAction *wakeAct = menu.addAction(tr("Wake-on-LAN"));
    QAction *viewHistory = menu.addAction(tr("查看在线历史"));

    QAction *chosen = menu.exec(senderList->mapToGlobal(pos));
    if (chosen == copyMac) {
        QApplication::clipboard()->setText(macLbl->text());
        statusLabel->setText(tr("已复制 MAC: ") + macLbl->text());
    } else if (chosen == copyIp) {
        QApplication::clipboard()->setText(ipLbl->text());
        statusLabel->setText(tr("已复制: ") + ipLbl->text());
    } else if (chosen == copyAll) {
        QApplication::clipboard()->setText(ipLbl->text() + "\t" + macLbl->text());
        statusLabel->setText(tr("已复制: ") + ipLbl->text());
    } else if (chosen == portScan) {
        QString ip = ipLbl->text();
        if (!ip.isEmpty()) scanPorts(ip);
    } else if (chosen == pingAct) {
        QString ip = ipLbl->text();
        if (!ip.isEmpty()) pingDevice(ip);
    } else if (chosen == traceAct) {
        QString ip = ipLbl->text();
        if (!ip.isEmpty()) tracerouteDevice(ip);
    } else if (chosen == wakeAct) {
        QString mac = macLbl->text();
        QString ip = ipLbl->text();
        if (!mac.isEmpty()) wakeDevice(mac, ip);
    } else if (chosen == viewHistory) {
        QString key = item->data(Qt::UserRole).toString();
        viewDeviceHistory(key);
    }
}

void MainWindow::copySelected() {
    QListWidgetItem *item = lanList->currentItem();
    QListWidget *activeList = lanList;
    if (!item) {
        item = wifiList->currentItem();
        activeList = wifiList;
    }
    if (!item) return;
    QWidget *w = activeList->itemWidget(item);
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

// ==================== Export & Filter ====================

void MainWindow::exportResults() {
    int totalCount = lanList->count() + wifiList->count();
    if (totalCount == 0) {
        statusLabel->setText(tr("没有设备可导出"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("导出扫描结果"),
        QString(), "CSV (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        statusLabel->setText(tr("无法创建文件"));
        return;
    }

    QTextStream out(&file);
    out << "Type,IP/SSID,MAC,Latency/Signal,Manufacturer\n";

    auto exportList = [&](QListWidget *list) {
        for (int i = 0; i < list->count(); i++) {
            QListWidgetItem *item = list->item(i);
            QWidget *w = list->itemWidget(item);
            if (!w) continue;

            QLabel *ipLbl = w->findChild<QLabel*>("cardSsid");
            QLabel *macLbl = w->findChild<QLabel*>("cardMac");
            QLabel *vendorLbl = w->findChild<QLabel*>("cardVendor");
            QLabel *signalLbl = w->findChild<QLabel*>("cardSignal");

            QString key = item->data(Qt::UserRole).toString();
            QString type = key.startsWith("LAN:") ? "LAN" : "WiFi";

            out << type << ","
                << (ipLbl ? ipLbl->text() : "") << ","
                << (macLbl ? macLbl->text() : "") << ","
                << (signalLbl ? signalLbl->text() : "") << ","
                << (vendorLbl ? vendorLbl->text() : "") << "\n";
        }
    };
    exportList(lanList);
    exportList(wifiList);

    file.close();
    statusLabel->setText(tr("已导出 %1 条记录到 %2").arg(totalCount).arg(fileName));
}

void MainWindow::filterDevices(const QString &text) {
    QString query = text.trimmed().toLower();

    auto filterList = [&](QListWidget *list) {
        for (int i = 0; i < list->count(); i++) {
            QListWidgetItem *item = list->item(i);
            QWidget *w = list->itemWidget(item);
            if (!w) continue;

            if (query.isEmpty()) {
                item->setHidden(false);
                continue;
            }

            QLabel *ipLbl = w->findChild<QLabel*>("cardSsid");
            QLabel *macLbl = w->findChild<QLabel*>("cardMac");
            QLabel *vendorLbl = w->findChild<QLabel*>("cardVendor");

            bool match = false;
            if (ipLbl && ipLbl->text().toLower().contains(query)) match = true;
            if (macLbl && macLbl->text().toLower().contains(query)) match = true;
            if (vendorLbl && vendorLbl->text().toLower().contains(query)) match = true;

            item->setHidden(!match);
        }
    };
    filterList(lanList);
    filterList(wifiList);
}

// ==================== Topology & History ====================

void MainWindow::viewDeviceHistory(const QString &key) {
    if (!deviceHistory.contains(key) || deviceHistory[key].isEmpty()) {
        statusLabel->setText(tr("暂无历史记录"));
        return;
    }

    QStringList entries = deviceHistory[key];
    QString historyText = tr("设备 %1 在线历史：\n\n").arg(key);
    for (const QString &entry : entries) {
        historyText += entry + "\n";
    }

    QMessageBox::information(this, tr("在线历史"), historyText);
}

// ==================== Port Scan ====================

void MainWindow::scanPorts(const QString &ip) {
    if (ip.isEmpty() || ip == tr("未知")) {
        statusLabel->setText(tr("无法扫描：无效 IP"));
        return;
    }

    statusLabel->setText(tr("正在扫描端口: %1 ...").arg(ip));

    QtConcurrent::run([this, ip]() {
        QList<int> commonPorts = {22, 80, 443, 8080, 8443, 21, 23, 25, 53, 110, 143, 3306, 3389, 5900};
        QList<int> openPorts;

        for (int port : commonPorts) {
            QTcpSocket socket;
            socket.connectToHost(ip, port);
            if (socket.waitForConnected(300)) {
                openPorts.append(port);
                socket.disconnectFromHost();
            }
        }

        QTimer::singleShot(0, this, [this, ip, openPorts]() {
            if (openPorts.isEmpty()) {
                statusLabel->setText(tr("端口扫描完成: %1 - 未发现开放端口").arg(ip));
            } else {
                QStringList portStrs;
                for (int p : openPorts) portStrs << QString::number(p);
                statusLabel->setText(tr("端口扫描完成: %1 - 开放端口: %2").arg(ip).arg(portStrs.join(", ")));
            }
        });
    });
}

// ==================== Import/Export ====================

void MainWindow::importResults() {
    QString filePath = QFileDialog::getOpenFileName(this, tr("导入扫描结果"),
        QString(), "CSV (*.csv);;All Files (*)");
    if (filePath.isEmpty()) return;

    QList<NetworkDevice> imported = ScanImporter::importCsv(filePath);
    if (imported.isEmpty()) {
        statusLabel->setText(tr("导入失败或文件为空"));
        return;
    }

    // Merge with existing devices
    QList<NetworkDevice> existing;
    for (int i = 0; i < lanList->count(); i++) {
        QWidget *w = lanList->itemWidget(lanList->item(i));
        if (!w) continue;
        NetworkDevice dev;
        dev.type = Lan;
        dev.ip = w->findChild<QLabel*>("cardSsid")->text();
        dev.mac = w->findChild<QLabel*>("cardMac")->text();
        existing.append(dev);
    }
    for (int i = 0; i < wifiList->count(); i++) {
        QWidget *w = wifiList->itemWidget(wifiList->item(i));
        if (!w) continue;
        NetworkDevice dev;
        dev.type = WiFi;
        dev.ip = w->findChild<QLabel*>("cardSsid")->text();
        dev.mac = w->findChild<QLabel*>("cardMac")->text();
        existing.append(dev);
    }

    QList<NetworkDevice> merged = ScanImporter::mergeDevices(existing, imported);

    // Clear and rebuild lists
    lanList->clear();
    wifiList->clear();
    discoveredDevices.clear();

    for (const NetworkDevice &dev : merged) {
        QWidget *card = createDeviceCard(dev);
        QListWidgetItem *item = new QListWidgetItem();
        QString key = dev.type + ":" + (dev.mac.isEmpty() ? dev.ip : dev.mac);
        item->setData(Qt::UserRole, key);
        item->setSizeHint(card->sizeHint());
        discoveredDevices.insert(key);

        QListWidget *targetList = (dev.type == Lan) ? lanList : wifiList;
        targetList->addItem(item);
        targetList->setItemWidget(item, card);
    }

    statusLabel->setText(tr("已导入 %1 个设备，总计 %2 个").arg(imported.size()).arg(merged.size()));
}

// ==================== Speed Test ====================

void MainWindow::runSpeedTest() {
    statusLabel->setText(tr("正在测速..."));

    SpeedTest *test = new SpeedTest(this);
    connect(test, &SpeedTest::progress, this, [this](const QString &status) {
        statusLabel->setText(status);
    });
    connect(test, &SpeedTest::resultReady, this, [this, test](const SpeedTest::Result &result) {
        statusLabel->setText(tr("下载: %1 Mbps | 上传: %2 Mbps | 延迟: %3 ms")
            .arg(result.downloadMbps, 0, 'f', 2)
            .arg(result.uploadMbps, 0, 'f', 2)
            .arg(result.latencyMs));
        test->deleteLater();
    });
    connect(test, &SpeedTest::error, this, [this, test](const QString &msg) {
        statusLabel->setText(tr("测速失败: %1").arg(msg));
        test->deleteLater();
    });

    QtConcurrent::run([test]() {
        test->start();
    });
}

// ==================== Wake-on-LAN ====================

void MainWindow::wakeDevice(const QString &mac, const QString &ip) {
    statusLabel->setText(tr("正在唤醒 %1 ...").arg(ip));

    QtConcurrent::run([this, mac, ip]() {
        bool success = WakeOnLan::wakeAndVerify(mac, ip);
        QTimer::singleShot(0, this, [this, ip, success]() {
            if (success) {
                statusLabel->setText(tr("设备 %1 已唤醒").arg(ip));
            } else {
                statusLabel->setText(tr("唤醒失败: %1 (设备可能不支持 WoL)").arg(ip));
            }
        });
    });
}

// ==================== Ping/Traceroute ====================

void MainWindow::pingDevice(const QString &ip) {
    statusLabel->setText(tr("正在 Ping %1 ...").arg(ip));

    NetDiag *diag = new NetDiag(this);
    connect(diag, &NetDiag::lineReady, this, [this](const QString &line) {
        statusLabel->setText(line.trimmed().left(80));
    });
    connect(diag, &NetDiag::finished, this, [this, diag](int code) {
        Q_UNUSED(code);
        diag->deleteLater();
    });

    diag->ping(ip, 4);
}

void MainWindow::tracerouteDevice(const QString &ip) {
    statusLabel->setText(tr("正在 Traceroute %1 ...").arg(ip));

    NetDiag *diag = new NetDiag(this);
    connect(diag, &NetDiag::lineReady, this, [this](const QString &line) {
        statusLabel->setText(line.trimmed().left(80));
    });
    connect(diag, &NetDiag::finished, this, [this, diag](int code) {
        Q_UNUSED(code);
        diag->deleteLater();
    });

    diag->traceroute(ip);
}

// ==================== Vulnerability Scan ====================

void MainWindow::scanVulnerabilities() {
    statusLabel->setText(tr("正在扫描漏洞..."));

    // Collect all devices and their ports
    QList<NetworkDevice> allDevices;
    QHash<QString, QList<int>> portMap;

    for (int i = 0; i < lanList->count(); i++) {
        QWidget *w = lanList->itemWidget(lanList->item(i));
        if (!w) continue;
        NetworkDevice dev;
        dev.type = Lan;
        dev.ip = w->findChild<QLabel*>("cardSsid")->text();
        dev.mac = w->findChild<QLabel*>("cardMac")->text();
        allDevices.append(dev);
    }

    QtConcurrent::run([this, allDevices, portMap]() {
        VulnScanner &scanner = VulnScanner::instance();
        QList<VulnFinding> findings;

        // For each device, scan ports and check vulnerabilities
        for (const NetworkDevice &dev : allDevices) {
            QList<int> openPorts;
            QTcpSocket socket;
            QList<int> commonPorts = {22, 23, 21, 80, 443, 445, 554, 3389, 5900, 9100};

            for (int port : commonPorts) {
                socket.connectToHost(dev.ip, port);
                if (socket.waitForConnected(200)) {
                    openPorts.append(port);
                    socket.disconnectFromHost();
                }
            }

            QList<VulnFinding> devFindings = scanner.scanDevice(dev.ip, dev.mac, openPorts);
            findings.append(devFindings);
        }

        QTimer::singleShot(0, this, [this, findings]() {
            if (findings.isEmpty()) {
                statusLabel->setText(tr("未发现安全风险"));
            } else {
                int critical = 0, high = 0, medium = 0;
                for (const VulnFinding &f : findings) {
                    if (f.rule.severity == "Critical") critical++;
                    else if (f.rule.severity == "High") high++;
                    else if (f.rule.severity == "Medium") medium++;
                }
                statusLabel->setText(tr("发现 %1 个风险: 严重%2 高%3 中%4")
                    .arg(findings.size()).arg(critical).arg(high).arg(medium));
            }
        });
    });
}
