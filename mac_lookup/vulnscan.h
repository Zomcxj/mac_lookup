#ifndef VULNSCAN_H
#define VULNSCAN_H

#include <QString>
#include <QList>
#include <QMap>
#include "scanner.h"

struct VulnRule {
    QString id;
    QString name;
    QString severity;   // Low/Medium/High/Critical
    QString description;
    QString remediation;
};

struct VulnFinding {
    QString deviceId;
    QString deviceIp;
    QString deviceMac;
    VulnRule rule;
};

class VulnScanner {
public:
    static VulnScanner& instance();

    // Scan device for vulnerabilities
    QList<VulnFinding> scanDevice(const QString &ip, const QString &mac, const QList<int> &ports) const;

    // Scan all devices
    QList<VulnFinding> scanAll(const QList<NetworkDevice> &devices, const QHash<QString, QList<int>> &portMap) const;

    // Get all rules
    QList<VulnRule> rules() const { return m_rules; }

    // Get severity color
    static QString severityColor(const QString &severity);

private:
    VulnScanner();
    void loadRules();

    QList<VulnRule> m_rules;
};

#endif // VULNSCAN_H
