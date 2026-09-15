// device_card_factory.cpp - Device card creation and styling extracted from MainWindow
#include "device_card_factory.h"
#include "thememanager.h"
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>

static void styleCard(QFrame *card, QLabel *typeLbl, QLabel *ssidLbl, QLabel *macLbl, QLabel *vendorLbl, bool isDark, bool isLan) {
    card->setStyleSheet(ThemeManager::cardStyle(isLan, isDark));
    typeLbl->setStyleSheet(ThemeManager::typeLabelStyle(isLan, isDark));
    ssidLbl->setStyleSheet(ThemeManager::ssidStyle(isDark));
    macLbl->setStyleSheet(ThemeManager::macStyle(isDark));
    vendorLbl->setStyleSheet(ThemeManager::vendorStyle(isDark));
}

QWidget* DeviceCardFactory::createCard(const NetworkDevice &dev, bool isDark) {
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

    QLabel *ipLabel = new QLabel(dev.ip.isEmpty() ? QObject::tr("未知") : dev.ip);
    ipLabel->setObjectName("cardSsid");

    QString statusColor;
    QString statusText;
    if (dev.type == Lan) {
        if (dev.latency < 0) {
            statusColor = "#999999";
            statusText = QObject::tr("离线");
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

    QLabel *macLabel = new QLabel(dev.mac.isEmpty() ? QObject::tr("未知 MAC") : dev.mac);
    macLabel->setObjectName("cardMac");

    QLabel *vendorLabel = new QLabel(dev.manufacturer.isEmpty() ? QObject::tr("未知厂商") : dev.manufacturer);
    vendorLabel->setObjectName("cardVendor");

    bottomRow->addWidget(macLabel);
    bottomRow->addWidget(vendorLabel);
    bottomRow->addStretch();
    layout->addLayout(bottomRow);

    styleCard(card, typeLabel, ipLabel, macLabel, vendorLabel, isDark, dev.type == Lan);
    return card;
}

void DeviceCardFactory::updateAllStyles(QListWidget *lanList, QListWidget *wifiList, bool isDark) {
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
                styleCard(card, typeLbl, ssidLbl, macLbl, vendorLbl, isDark, card->objectName() == "lanCard");
            }
            if (signalLbl) {
                QString sc = signalLbl->styleSheet();
                QString c = sc.contains("#34a853") ? "#34a853" : sc.contains("#fbbc05") ? "#fbbc05" : sc.contains("#ea4335") ? "#ea4335" : "#999999";
                signalLbl->setStyleSheet(ThemeManager::signalStyle(c));
            }
        }
    }
}
