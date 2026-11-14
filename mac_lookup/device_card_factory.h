#ifndef DEVICE_CARD_FACTORY_H
#define DEVICE_CARD_FACTORY_H

#include <QWidget>
#include <QListWidget>
#include "scanner.h"

class DeviceCardFactory {
public:
    static QWidget* createCard(const NetworkDevice &dev, bool isDark);
    static void updateCard(QWidget *card, const NetworkDevice &dev);
    static void updateAllStyles(QListWidget *lanList, QListWidget *wifiList, bool isDark);
};

#endif // DEVICE_CARD_FACTORY_H
