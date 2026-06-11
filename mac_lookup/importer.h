#ifndef IMPORTER_H
#define IMPORTER_H

#include <QString>
#include <QList>
#include <QHash>
#include "scanner.h"

class ScanImporter {
public:
    // Import devices from CSV file
    static QList<NetworkDevice> importCsv(const QString &filePath);

    // Merge imported devices with existing devices
    // Returns merged list (existing + new, duplicates updated)
    static QList<NetworkDevice> mergeDevices(
        const QList<NetworkDevice> &existing,
        const QList<NetworkDevice> &imported
    );

    // Export devices to CSV file
    static bool exportCsv(const QString &filePath, const QList<NetworkDevice> &devices);

private:
    static NetworkDevice parseCsvLine(const QString &line);
};

#endif // IMPORTER_H
