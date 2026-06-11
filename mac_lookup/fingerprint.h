#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <QString>
#include <QList>
#include <QMap>

// Device type enumeration
enum DeviceCategory {
    Unknown,
    Router,
    Phone,
    Laptop,
    Desktop,
    Printer,
    IoT,
    Camera,
    SmartTV,
    GameConsole,
    Server,
    NAS
};

// Device fingerprint result
struct DeviceFingerprint {
    DeviceCategory category;
    QString icon;       // Unicode icon
    QString typeName;   // Display name
    QString riskLevel;  // Low/Medium/High/Critical
    QString riskReason;
};

class FingerprintDB {
public:
    static FingerprintDB& instance();

    // Classify device by MAC prefix
    DeviceCategory classifyByMac(const QString &mac) const;

    // Classify device by open ports
    DeviceCategory classifyByPorts(const QList<int> &ports) const;

    // Get full fingerprint
    DeviceFingerprint fingerprint(const QString &mac, const QList<int> &ports) const;

    // Get icon for category
    static QString iconForCategory(DeviceCategory cat);

    // Get name for category
    static QString nameForCategory(DeviceCategory cat);

private:
    FingerprintDB();
    void loadOuiCategories();

    QMap<QString, DeviceCategory> ouiMap;  // MAC prefix -> category
};

#endif // FINGERPRINT_H
