// wol.cpp - Wake-on-LAN implementation
#include "wol.h"
#include <QUdpSocket>
#include <QProcess>
#include <QThread>

QByteArray WakeOnLan::buildMagicPacket(const QString &mac) {
    // Magic packet: 6 bytes of 0xFF + MAC repeated 16 times
    QByteArray packet;
    packet.fill(static_cast<char>(0xFF), 6);

    // Parse MAC address
    QString cleanMac = mac;
    cleanMac.replace(":", "").replace("-", "");
    cleanMac = cleanMac.toUpper();
    if (cleanMac.length() != 12)
        return QByteArray();

    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 6; j++) {
            bool ok;
            quint8 byte = cleanMac.mid(j * 2, 2).toUInt(&ok, 16);
            if (!ok) return QByteArray();
            packet.append(static_cast<char>(byte));
        }
    }

    return packet;
}

bool WakeOnLan::sendMagicPacket(const QString &mac, const QString &broadcast) {
    QByteArray packet = buildMagicPacket(mac);
    if (packet.isEmpty())
        return false;

    QUdpSocket socket;
    // Send to port 9 (standard WoL port)
    qint64 sent = socket.writeDatagram(packet, QHostAddress(broadcast), 9);
    return sent == packet.size();
}

bool WakeOnLan::isDeviceAwake(const QString &ip, int timeoutMs) {
    QProcess ping;
#ifdef Q_OS_WIN
    ping.start("ping", QStringList() << "-n" << "1" << "-w" << QString::number(timeoutMs) << ip);
#else
    ping.start("ping", QStringList() << "-c" << "1" << "-W" << QString::number(timeoutMs / 1000) << ip);
#endif
    ping.waitForFinished(timeoutMs + 500);
    return ping.exitCode() == 0;
}

bool WakeOnLan::wakeAndVerify(const QString &mac, const QString &ip, const QString &broadcast) {
    // Send magic packet
    if (!sendMagicPacket(mac, broadcast))
        return false;

    // Wait and check if device wakes up
    for (int i = 0; i < 5; i++) {
        QThread::sleep(1);
        if (isDeviceAwake(ip))
            return true;
    }

    return false;
}
