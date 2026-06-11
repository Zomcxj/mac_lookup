// fingerprint.cpp - Device fingerprinting and classification
#include "fingerprint.h"

FingerprintDB& FingerprintDB::instance() {
    static FingerprintDB db;
    return db;
}

FingerprintDB::FingerprintDB() {
    loadOuiCategories();
}

void FingerprintDB::loadOuiCategories() {
    // Common OUI prefixes for device classification
    // Routers
    ouiMap["00:1A:2B"] = Router; ouiMap["00:1E:58"] = Router;
    ouiMap["00:24:D4"] = Router; ouiMap["00:26:F2"] = Router;
    ouiMap["C0:25:E9"] = Router; ouiMap["D8:50:E6"] = Router;
    ouiMap["E4:F0:04"] = Router; ouiMap["F8:1A:67"] = Router;

    // Phones (Apple, Samsung, Huawei, Xiaomi)
    ouiMap["00:1B:63"] = Phone; ouiMap["AC:CF:5C"] = Phone;
    ouiMap["F0:D7:AA"] = Phone; ouiMap["8C:8E:F2"] = Phone;
    ouiMap["00:21:9B"] = Phone; ouiMap["00:26:08"] = Phone;
    ouiMap["00:23:76"] = Phone; ouiMap["00:25:BC"] = Phone;

    // Laptops (Dell, HP, Lenovo, Asus)
    ouiMap["00:14:22"] = Laptop; ouiMap["00:1A:A0"] = Laptop;
    ouiMap["00:1E:4F"] = Laptop; ouiMap["00:24:E8"] = Laptop;
    ouiMap["00:25:B3"] = Laptop; ouiMap["00:26:22"] = Laptop;
    ouiMap["00:19:DB"] = Laptop; ouiMap["00:1C:BF"] = Laptop;

    // Printers (HP, Canon, Epson)
    ouiMap["00:1E:0B"] = Printer; ouiMap["00:25:B1"] = Printer;
    ouiMap["00:1E:8F"] = Printer; ouiMap["00:26:AB"] = Printer;

    // IoT devices
    ouiMap["00:17:88"] = IoT; ouiMap["00:21:CC"] = IoT;
    ouiMap["00:24:E4"] = IoT; ouiMap["AC:CF:23"] = IoT;

    // Cameras
    ouiMap["00:12:1C"] = Camera; ouiMap["00:1A:07"] = Camera;
    ouiMap["00:1E:4A"] = Camera; ouiMap["00:26:7B"] = Camera;
}

DeviceCategory FingerprintDB::classifyByMac(const QString &mac) const {
    QString prefix = mac.left(8).toUpper();
    if (ouiMap.contains(prefix))
        return ouiMap[prefix];
    return Unknown;
}

DeviceCategory FingerprintDB::classifyByPorts(const QList<int> &ports) const {
    // Port-based classification
    if (ports.contains(80) || ports.contains(443)) {
        if (ports.contains(8080) || ports.contains(8443))
            return Router;  // Web admin interface
        if (ports.contains(554) || ports.contains(8554))
            return Camera;  // RTSP
        return Server;
    }
    if (ports.contains(9100) || ports.contains(631))
        return Printer;
    if (ports.contains(554) || ports.contains(8554))
        return Camera;
    if (ports.contains(22) && !ports.contains(80))
        return Server;
    if (ports.contains(3389) || ports.contains(5900))
        return Desktop;
    if (ports.contains(1883) || ports.contains(8883))
        return IoT;  // MQTT
    if (ports.contains(53))
        return Router;
    return Unknown;
}

DeviceFingerprint FingerprintDB::fingerprint(const QString &mac, const QList<int> &ports) const {
    DeviceFingerprint fp;

    // Try MAC-based classification first
    fp.category = classifyByMac(mac);

    // If unknown, try port-based
    if (fp.category == Unknown && !ports.isEmpty())
        fp.category = classifyByPorts(ports);

    fp.icon = iconForCategory(fp.category);
    fp.typeName = nameForCategory(fp.category);

    // Risk assessment
    if (ports.contains(23)) {
        fp.riskLevel = "Critical";
        fp.riskReason = "Telnet 开放（明文传输）";
    } else if (ports.contains(445)) {
        fp.riskLevel = "High";
        fp.riskReason = "SMB 端口开放（潜在漏洞）";
    } else if (ports.contains(3389)) {
        fp.riskLevel = "High";
        fp.riskReason = "RDP 端口开放（暴力破解风险）";
    } else if (ports.contains(21)) {
        fp.riskLevel = "Medium";
        fp.riskReason = "FTP 端口开放（明文认证）";
    } else if (ports.contains(22)) {
        fp.riskLevel = "Low";
        fp.riskReason = "SSH 端口开放（安全）";
    } else if (ports.isEmpty()) {
        fp.riskLevel = "Low";
        fp.riskReason = "无开放端口";
    } else {
        fp.riskLevel = "Low";
        fp.riskReason = "常规端口";
    }

    return fp;
}

QString FingerprintDB::iconForCategory(DeviceCategory cat) {
    switch (cat) {
        case Router:    return "🌐";
        case Phone:     return "📱";
        case Laptop:    return "💻";
        case Desktop:   return "🖥️";
        case Printer:   return "🖨️";
        case IoT:       return "📡";
        case Camera:    return "📷";
        case SmartTV:   return "📺";
        case GameConsole: return "🎮";
        case Server:    return "🖧";
        case NAS:       return "💾";
        default:        return "❓";
    }
}

QString FingerprintDB::nameForCategory(DeviceCategory cat) {
    switch (cat) {
        case Router:    return "路由器";
        case Phone:     return "手机";
        case Laptop:    return "笔记本";
        case Desktop:   return "台式机";
        case Printer:   return "打印机";
        case IoT:       return "IoT 设备";
        case Camera:    return "摄像头";
        case SmartTV:   return "智能电视";
        case GameConsole: return "游戏主机";
        case Server:    return "服务器";
        case NAS:       return "NAS";
        default:        return "未知设备";
    }
}
