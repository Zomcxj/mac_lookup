# 编译与打包指南

## 环境要求

### Windows
- Qt 5.12.12 MinGW 64-bit
- MinGW 7.3（随 Qt 安装）
- Git Bash / MSYS2
- Enigma Virtual Box（单文件打包）

### Linux / ARM
- Qt 5.12 开发包（`qt5-qmake`, `qtbase5-dev`, `qttools5-dev-tools`）
- GCC / G++
- `linuxdeployqt` + `appimagetool`（可选，用于 AppImage 打包）

---

## Windows 编译

### 1. 编译

在项目根目录打开 Git Bash：

```bash
./build-mac_lookup.sh              # Release 完整构建
./build-mac_lookup.sh fast         # 快速构建（增量编译）
./build-mac_lookup.sh debug        # Debug 构建
```

### 2. 输出目录

```
build-mac_lookup/
├── mac_lookup.exe      # 可执行文件
├── Qt5Core.dll         # Qt 动态库（自动部署）
├── Qt5Gui.dll
├── Qt5Widgets.dll
├── Qt5Network.dll
├── platforms/          # Qt 平台插件
├── data/               # 应用数据
│   └── mac_database.txt
└── ...
```

### 3. 打包单文件 exe（Enigma Virtual Box）

将 `build-mac_lookup/` 整个目录打包为单个可执行文件：

1. 打开 Enigma Virtual Box
2. **Enter Input File Name** → 选择 `build-mac_lookup\mac_lookup.exe`
3. **Enter Output File Name** → 设置输出路径，如 `mac_lookup_packed.exe`
4. 点击 **Add** → **Add Folder** → 选择 `build-mac_lookup` 目录
5. **Destination folder** 留空（默认 `%DEFAULT FOLDER%`）
6. **Options** 选项卡设置：
   | 设置项 | 推荐值 | 说明 |
   |--------|--------|------|
   | Compression | Normal / High | 压缩率与启动速度平衡 |
   | Temporary Files | Do not use | 避免释放临时文件到磁盘 |
   | Execution Level | Require Administrator | ARP 扫描需要管理员权限 |
7. 点击 **Process** 开始打包
8. 输出单文件 exe，可拷贝到任意 Windows 64 位机器运行

#### 打包验证

- [ ] MAC 地址查询正常
- [ ] 扫描功能正常
- [ ] 数据库加载正常（状态栏显示记录数）
- [ ] ARP 扫描需以管理员身份运行

---

## Linux / ARM 编译

### 1. 安装依赖

```bash
sudo apt install qt5-qmake qtbase5-dev qttools5-dev-tools build-essential
```

### 2. 编译

```bash
./build-mac_lookup.sh              # Release 构建
./build-mac_lookup.sh fast         # 快速构建
```

### 3. 直接运行

```bash
./build-mac_lookup/mac_lookup
```

### 4. 打包 AppImage

```bash
./deploy-mac_lookup.sh
```

输出：`deploy-mac_lookup/mac_lookup-x86_64.AppImage`

AppImage 为单文件可执行包，无需安装即可在大多数 Linux 系统运行。

#### 打包依赖

将以下工具放到项目根目录或设置环境变量：

| 工具 | 说明 |
|------|------|
| `linuxdeployqt-continuous-x86_64.AppImage` | Qt 库打包 |
| `appimagetool-x86_64.AppImage` | AppImage 生成 |

下载地址：
- https://github.com/probonopd/linuxdeployqt/releases
- https://github.com/AppImage/AppImageKit/releases

---

## 常见问题

### Q: 编译报 `Qt::SkipEmptyParts` 错误

Qt 5.14 以下版本不支持 `Qt::SkipEmptyParts`。代码已做版本兼容处理，确保使用最新源码。

### Q: Windows 编译报 `g++ not found`

确保 Qt MinGW 和 Tools 路径在 PATH 中。构建脚本会自动检测 `D:\software\Qt\Qt512\` 下的安装路径。

### Q: Linux 打包 AppImage 报 `icon not found`

确保 `mac_lookup/data/mac_lookup.png` 存在（256x256 像素 PNG）。

### Q: 窗口标题/界面中文乱码

源文件以 UTF-8 保存，`.pro` 文件包含 `CONFIG += utf8_source`。如仍有问题，执行 clean 构建：

```bash
./build-mac_lookup.sh    # 不加 fast，完整清理重建
```

### Q: Enigma Virtual Box 打包后启动闪退

- 检查 `platforms\qwindows.dll` 是否已添加
- 检查是否遗漏 MinGW 运行时 DLL
- 确认 `data\mac_database.txt` 已添加到打包列表
