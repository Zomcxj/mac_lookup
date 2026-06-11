#ifndef WOL_H
#define WOL_H

#include <QString>

class WakeOnLan {
public:
    // Send magic packet to wake device
    static bool sendMagicPacket(const QString &mac, const QString &broadcast = "255.255.255.255");

    // Check if device is awake (ping test)
    static bool isDeviceAwake(const QString &ip, int timeoutMs = 2000);

    // Wake device and verify
    static bool wakeAndVerify(const QString &mac, const QString &ip, const QString &broadcast = "255.255.255.255");

private:
    static QByteArray buildMagicPacket(const QString &mac);
};

#endif // WOL_H
