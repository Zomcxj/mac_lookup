# MAC Lookup

MAC 地址查询与网络扫描工具，支持局域网 (ARP) 和 WiFi 扫描。

## 功能

- **局域网扫描**：通过 ARP 协议发现局域网设备，显示 IP、MAC 地址和厂商信息
- **WiFi 扫描**：扫描周围无线网络，显示 SSID、MAC 地址、信号强度和厂商
- **OUI 数据库**：内置 11,000+ 条 CN/TW 区域 MAC 厂商记录，自动识别设备制造商
- **实时扫描**：支持持续扫描模式，动态显示设备上下线
- **卡片式列表**：LAN 设备蓝色标识，WiFi 设备绿色标识
- **一键复制**：右键菜单快速复制 MAC 地址、IP/SSID 或全部信息

## 平台支持

| 平台 | 编译器 | 扫描方式 | 打包方式 |
|------|--------|----------|----------|
| Windows | MinGW 7.3 (Qt 5.12) | `GetIpNetTable` + WLAN API | Enigma Virtual Box（单文件 exe） |
| Linux/ARM | GCC | `/proc/net/arp` + `iwlist` | AppImage（单文件） |

## 快速开始

```bash
# 构建
./build-mac_lookup.sh              # Release 完整构建
./build-mac_lookup.sh fast         # 快速构建（增量编译）
./build-mac_lookup.sh debug        # Debug 构建

# 运行
./build-mac_lookup/mac_lookup      # Linux
build-mac_lookup\mac_lookup.exe    # Windows
```

## 技术栈

- **Qt 5.12** — C++ GUI 框架
- **QtConcurrent** — 多线程扫描
- **QtNetwork** — 网络接口枚举
- **QRegularExpression** — 日志解析

## 许可证

[MIT License](LICENSE)
