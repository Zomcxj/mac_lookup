// mainwindow.cpp - UI and main logic
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
#include <QFileDialog>
#include <QDateTime>
#include <QMessageBox>
#include <QDialog>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
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
    scanTimer = new QTimer(this);
    scanTimer->setSingleShot(false);
    connect(scanTimer, &QTimer::timeout, this, &MainWindow::doScan);
    setupUI();
    applyStyles();

    QString defaultPath = QApplication::applicationDirPath() + "/data/mac_database.txt";
    ouiDb.load(defaultPath);
    statusLabel->setText(tr("已加载 %1 条记录 (含 %2 家WiFi厂商)")
                         .arg(ouiDb.size()).arg(ouiDb.wifiVendorCount()));

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

    QPushButton *topologyButton = new QPushButton(tr("拓扑"), scanCard);
    topologyButton->setObjectName("topologyButton");
    topologyButton->setCursor(Qt::PointingHandCursor);
    topologyButton->setFixedHeight(40);
    topologyButton->setFixedWidth(60);
    btnRow->addWidget(topologyButton);

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

    wifiList = new QListWidget(scanCard);
    wifiList->setObjectName("wifiList");
    wifiList->setSelectionMode(QAbstractItemView::SingleSelection);
    wifiList->setContextMenuPolicy(Qt::CustomContextMenu);
    wifiList->setMinimumHeight(120);
    wifiList->setMinimumWidth(320);
    wifiList->setSpacing(4);
    wifiList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

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
    connect(topologyButton, &QPushButton::clicked, this, &MainWindow::showTopology);
    connect(lanList, &QWidget::customContextMenuRequested, this, &MainWindow::onListContextMenu);
    connect(wifiList, &QWidget::customContextMenuRequested, this, &MainWindow::onListContextMenu);

    QShortcut *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    connect(copyShortcut, &QShortcut::activated, this, &MainWindow::copySelected);

    // Style for LAN/WiFi list headers
    lanLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #4a90d9; padding: 2px;");
    wifiLabel->setStyleSheet("font-size: 13px; font-weight: bold; color: #34a853; padding: 2px;");
}

void MainWindow::applyStyles() {
    setStyleSheet(R"(
        QMainWindow {
            background: transparent;
        }
        #centralWidget { background: transparent; }
        #titleBar {
            background-color: #f0f0f0;
            border-top-left-radius: 12px; border-top-right-radius: 12px;
            border-bottom: 1px solid #d0d0d0;
        }
        #titleText { color: #333333; font-size: 13px; font-weight: bold; background: transparent; }
        #minBtn, #maxBtn, #closeBtn {
            background: transparent; border: none; font-size: 14px; color: #666666;
            border-radius: 4px;
        }
        #minBtn:hover, #maxBtn:hover { background-color: #e0e0e0; }
        #closeBtn:hover { background-color: #e81123; color: #ffffff; }
        #titleLabel { color: #2d2d2d; font-size: 18px; font-weight: bold; }
        #scanCard { background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 10px; }
        #scanButton {
            background-color: #34a853; color: #ffffff; border: none;
            border-radius: 8px; font-size: 14px; font-weight: bold;
        }
        #scanButton:hover { background-color: #2d9248; }
        #scanButton:pressed { background-color: #267e3e; }
        #lanList, #wifiList { background-color: transparent; border: none; font-size: 12px; }
        #lanList::item, #wifiList::item { padding: 0px; border: none; background: transparent; }
        #filterInput {
            background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 6px;
            padding: 4px 10px; font-size: 13px; color: #333333;
        }
        #filterInput:focus { border-color: #4a90d9; }
        #exportButton {
            background-color: #4a90d9; color: #ffffff; border: none;
            border-radius: 8px; font-size: 13px;
        }
        #exportButton:hover { background-color: #357abd; }
        #darkModeButton {
            background-color: #e0e0e0; color: #333333; border: none;
            border-radius: 8px; font-size: 13px;
        }
        #darkModeButton:hover { background-color: #d0d0d0; }
        #topologyButton {
            background-color: #e0e0e0; color: #333333; border: none;
            border-radius: 8px; font-size: 13px;
        }
        #topologyButton:hover { background-color: #d0d0d0; }
        #deviceMacLabel { color: #666666; font-size: 12px; font-family: 'Consolas', monospace; }
        #deviceMacLabel:hover { color: #4a90d9; }
        #statusLabel { color: #666666; font-size: 12px; }
        QStatusBar {
            background: #f0f0f0;
            border-top: 1px solid #d0d0d0;
            border-bottom-left-radius: 12px; border-bottom-right-radius: 12px;
            min-height: 22px;
        }
        QStatusBar::item { border: none; }
    )");
}

void MainWindow::toggleDarkMode() {
    darkMode = !darkMode;
    if (darkMode) {
        setStyleSheet(R"(
            QMainWindow {
                background: transparent;
            }
            #centralWidget { background: transparent; }
            #titleBar {
                background-color: #2d2d2d;
                border-top-left-radius: 12px; border-top-right-radius: 12px;
                border-bottom: 1px solid #3d3d3d;
            }
            #titleText { color: #e0e0e0; font-size: 13px; font-weight: bold; }
            #minBtn, #maxBtn, #closeBtn {
                background: transparent; border: none; font-size: 14px; color: #999999;
                border-radius: 4px;
            }
            #minBtn:hover, #maxBtn:hover { background-color: #3d3d3d; }
            #closeBtn:hover { background-color: #e81123; color: #ffffff; }
            #titleLabel { color: #e0e0e0; font-size: 18px; font-weight: bold; }
            #scanCard { background-color: #2d2d2d; border: 1px solid #3d3d3d; border-radius: 10px; }
            #scanButton {
                background-color: #34a853; color: #ffffff; border: none;
                border-radius: 8px; font-size: 14px; font-weight: bold;
            }
            #scanButton:hover { background-color: #2d9248; }
            #scanButton:pressed { background-color: #267e3e; }
            #lanList, #wifiList { background-color: #252525; border: none; font-size: 12px; color: #e0e0e0; }
            #lanList::item, #wifiList::item { padding: 0px; border: none; background: transparent; }
            #filterInput {
                background-color: #3d3d3d; border: 1px solid #4d4d4d; border-radius: 6px;
                padding: 4px 10px; font-size: 13px; color: #e0e0e0;
            }
            #filterInput:focus { border-color: #4a90d9; }
            #exportButton {
                background-color: #4a90d9; color: #ffffff; border: none;
                border-radius: 8px; font-size: 13px;
            }
            #exportButton:hover { background-color: #357abd; }
            #darkModeButton {
                background-color: #555555; color: #e0e0e0; border: none;
                border-radius: 8px; font-size: 13px;
            }
            #darkModeButton:hover { background-color: #666666; }
            #topologyButton {
                background-color: #555555; color: #e0e0e0; border: none;
                border-radius: 8px; font-size: 13px;
            }
            #topologyButton:hover { background-color: #666666; }
            #deviceMacLabel { color: #999999; font-size: 12px; font-family: 'Consolas', monospace; }
            #deviceMacLabel:hover { color: #4a90d9; }
            #statusLabel { color: #999999; font-size: 12px; }
            QStatusBar {
                background: #2d2d2d; border-top: 1px solid #3d3d3d;
                border-bottom-left-radius: 12px; border-bottom-right-radius: 12px;
                min-height: 22px;
            }
            QStatusBar::item { border: none; }
            QSpinBox { background-color: #3d3d3d; color: #e0e0e0; border: 1px solid #4d4d4d; border-radius: 4px; }
            QLabel { color: #e0e0e0; }
            QLabel#intervalLabel { color: #999999; font-size: 12px; }
            QGraphicsView { background-color: #252525; border: none; }
        )");
        // Update LAN/WiFi header colors for dark mode
        QList<QLabel*> labels = scanCard->findChildren<QLabel*>();
        for (QLabel *lbl : labels) {
            if (lbl->text().contains("LAN"))
                lbl->setStyleSheet("font-size: 13px; font-weight: bold; color: #64b5f6; padding: 2px; background: transparent;");
            else if (lbl->text().contains("WiFi"))
                lbl->setStyleSheet("font-size: 13px; font-weight: bold; color: #81c784; padding: 2px; background: transparent;");
        }
    } else {
        applyStyles();
        // Restore LAN/WiFi header colors
        QList<QLabel*> labels = scanCard->findChildren<QLabel*>();
        for (QLabel *lbl : labels) {
            if (lbl->text().contains("LAN"))
                lbl->setStyleSheet("font-size: 13px; font-weight: bold; color: #4a90d9; padding: 2px;");
            else if (lbl->text().contains("WiFi"))
                lbl->setStyleSheet("font-size: 13px; font-weight: bold; color: #34a853; padding: 2px;");
        }
    }
}

// ==================== Scan Control ====================

void MainWindow::startScan() {
    if (!isScanning) {
        isScanning = true;
        discoveredDevices.clear();
        lanList->clear();
        wifiList->clear();
        scanButton->setText(tr("⏹ 停止扫描"));
        scanButton->setStyleSheet("#scanButton { background-color: #ea4335; color: #ffffff; border: none; border-radius: 8px; font-size: 14px; font-weight: bold; }");
        scanTimer->start(intervalSpin->value() * 1000);
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
            if (dev.type == "LAN") lanCount++;
            else wifiCount++;
            continue;
        }
        discoveredDevices.insert(key);

        if (dev.type == "LAN") lanCount++;
        else wifiCount++;

        deviceHistory[key].append(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") + " online");

        QWidget *card = createDeviceCard(dev);
        QListWidgetItem *item = new QListWidgetItem();
        item->setData(Qt::UserRole, key);
        item->setSizeHint(card->sizeHint());

        QListWidget *targetList = (dev.type == "LAN") ? lanList : wifiList;
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
        if (!ip.isEmpty()) {
            scanPorts(ip);
        }
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

void MainWindow::showTopology() {
    int totalCount = lanList->count() + wifiList->count();
    if (totalCount == 0) {
        statusLabel->setText(tr("没有设备可显示"));
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("网络拓扑"));
    dialog.resize(600, 400);

    QGraphicsScene *scene = new QGraphicsScene(&dialog);
    QGraphicsView *view = new QGraphicsView(scene, &dialog);
    view->setRenderHint(QPainter::Antialiasing);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(view);

    QFont font("Arial", 8);
    QFontMetrics fm(font);

    // Local machine at center
    QGraphicsEllipseItem *localNode = scene->addEllipse(250, 180, 80, 40, QPen(Qt::blue), QBrush(Qt::lightGray));
    QGraphicsTextItem *localText = scene->addText(tr("本机"), font);
    localText->setTextWidth(70);
    localText->setPos(265, 188);

    int lanX = 20;
    int wifiX = 460;
    int y = 10;
    int nodeW = 110;
    int nodeH = 35;

    auto addNodes = [&](QListWidget *list, int x, bool isLan) {
        for (int i = 0; i < list->count(); i++) {
            QListWidgetItem *item = list->item(i);
            QWidget *w = list->itemWidget(item);
            if (!w) continue;

            QLabel *ipLbl = w->findChild<QLabel*>("cardSsid");
            if (!ipLbl) continue;

            QString label = ipLbl->text();
            QColor color = isLan ? QColor(74, 144, 217) : QColor(52, 168, 83);

            scene->addEllipse(x, y, nodeW, nodeH, QPen(color), QBrush(color.lighter(160)));

            QGraphicsTextItem *text = scene->addText(fm.elidedText(label, Qt::ElideRight, nodeW - 10), font);
            text->setTextWidth(nodeW - 10);
            text->setPos(x + 5, y + 8);

            int lineStartX = isLan ? x + nodeW : x;
            int lineEndX = isLan ? 250 : 330;
            scene->addLine(lineStartX, y + nodeH / 2, lineEndX, 200, QPen(Qt::gray, 1, Qt::DashLine));

            y += 45;
            if (y > 350) y = 10;
        }
    };

    addNodes(lanList, lanX, true);
    y = 10;
    addNodes(wifiList, wifiX, false);

    dialog.exec();
}

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
