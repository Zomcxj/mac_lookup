#ifndef UTILS_H
#define UTILS_H

#include <QString>

// Format 6 raw bytes into "XX:XX:XX:XX:XX:XX" uppercase hex string.
// Shared between scanner (Windows ARP scan) and tests.
inline QString formatMacAddress(const unsigned char *bytes, int len) {
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

#endif // UTILS_H
