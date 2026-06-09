#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QString>

class ThemeManager {
public:
    static QString lightSheet();
    static QString darkSheet();
    static QString cardStyle(bool isLan, bool isDark);
    static QString typeLabelStyle(bool isLan, bool isDark);
    static QString ssidStyle(bool isDark);
    static QString macStyle(bool isDark);
    static QString vendorStyle(bool isDark);
    static QString signalStyle(const QString &color);
};

#endif // THEMEMANAGER_H
