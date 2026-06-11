// vulnscan.cpp - Vulnerability scanning based on port patterns
#include "vulnscan.h"

VulnScanner& VulnScanner::instance() {
    static VulnScanner scanner;
    return scanner;
}

VulnScanner::VulnScanner() {
    loadRules();
}

void VulnScanner::loadRules() {
    // Common vulnerability rules based on port patterns
    m_rules = {
        {"TELNET-001", "Telnet 服务开放", "Critical",
         "Telnet 使用明文传输，凭证可被截获",
         "禁用 Telnet，改用 SSH"},

        {"SMB-001", "SMB 端口开放", "High",
         "SMB (445) 可能存在永恒之蓝等漏洞",
         "关闭不必要的 SMB 服务，更新系统补丁"},

        {"RDP-001", "远程桌面开放", "High",
         "RDP (3389) 暴力破解风险",
         "启用网络级认证(NLA)，限制访问IP"},

        {"FTP-001", "FTP 服务开放", "Medium",
         "FTP 使用明文认证",
         "改用 SFTP 或 FTPS"},

        {"HTTP-001", "HTTP 服务开放", "Low",
         "HTTP 未加密传输",
         "启用 HTTPS"},

        {"SSH-001", "SSH 服务开放", "Low",
         "SSH 服务正常开放",
         "确保使用强密码或密钥认证"},

        {"TELNET-002", "路由器 Telnet 开放", "Critical",
         "路由器管理接口通过 Telnet 暴露",
         "立即禁用 Telnet，改用 HTTPS 管理"},

        {"CAMERA-001", "摄像头 RTSP 开放", "High",
         "摄像头视频流未加密",
         "启用 RTSP over TLS，限制访问IP"},

        {"PRINTER-001", "打印机端口开放", "Medium",
         "打印机可能被远程访问",
         "限制打印机网络访问范围"},

        {"MQTT-001", "IoT MQTT 开放", "Medium",
         "IoT 设备 MQTT 未加密",
         "启用 MQTT TLS，设置认证"},

        {"VNC-001", "VNC 远程桌面开放", "High",
         "VNC 可能无认证或弱加密",
         "启用 VNC 加密，设置强密码"},

        {"DNS-001", "DNS 服务开放", "Medium",
         "DNS 可能被用于反射攻击",
         "限制 DNS 查询来源"},

        {"NETBIOS-001", "NetBIOS 端口开放", "High",
         "NetBIOS 可泄露系统信息",
         "禁用 NetBIOS over TCP/IP"}
    };
}

QList<VulnFinding> VulnScanner::scanDevice(const QString &ip, const QString &mac, const QList<int> &ports) const {
    QList<VulnFinding> findings;

    for (const VulnRule &rule : m_rules) {
        bool vulnerable = false;

        if (rule.id == "TELNET-001" && ports.contains(23))
            vulnerable = true;
        else if (rule.id == "SMB-001" && ports.contains(445))
            vulnerable = true;
        else if (rule.id == "RDP-001" && ports.contains(3389))
            vulnerable = true;
        else if (rule.id == "FTP-001" && ports.contains(21))
            vulnerable = true;
        else if (rule.id == "HTTP-001" && ports.contains(80))
            vulnerable = true;
        else if (rule.id == "SSH-001" && ports.contains(22))
            vulnerable = true;
        else if (rule.id == "TELNET-002" && ports.contains(23) && ports.contains(80))
            vulnerable = true;
        else if (rule.id == "CAMERA-001" && (ports.contains(554) || ports.contains(8554)))
            vulnerable = true;
        else if (rule.id == "PRINTER-001" && (ports.contains(9100) || ports.contains(631)))
            vulnerable = true;
        else if (rule.id == "MQTT-001" && (ports.contains(1883) || ports.contains(8883)))
            vulnerable = true;
        else if (rule.id == "VNC-001" && ports.contains(5900))
            vulnerable = true;
        else if (rule.id == "DNS-001" && ports.contains(53))
            vulnerable = true;
        else if (rule.id == "NETBIOS-001" && (ports.contains(139) || ports.contains(135)))
            vulnerable = true;

        if (vulnerable) {
            VulnFinding finding;
            finding.deviceIp = ip;
            finding.deviceMac = mac;
            finding.rule = rule;
            findings.append(finding);
        }
    }

    return findings;
}

QList<VulnFinding> VulnScanner::scanAll(
    const QList<NetworkDevice> &devices,
    const QHash<QString, QList<int>> &portMap) const
{
    QList<VulnFinding> allFindings;

    for (const NetworkDevice &dev : devices) {
        QString key = dev.mac.isEmpty() ? dev.ip : dev.mac;
        if (portMap.contains(key)) {
            QList<VulnFinding> findings = scanDevice(dev.ip, dev.mac, portMap[key]);
            allFindings.append(findings);
        }
    }

    return allFindings;
}

QString VulnScanner::severityColor(const QString &severity) {
    if (severity == "Critical") return "#ea4335";
    if (severity == "High") return "#fbbc05";
    if (severity == "Medium") return "#4a90d9";
    return "#34a853";  // Low
}
