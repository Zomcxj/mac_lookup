// thememanager.cpp - Theme and style management
//
// Provides consistent styling for light and dark modes.
// All colors follow Material Design guidelines:
//   - Blue (#4a90d9) for LAN elements
//   - Green (#34a853) for WiFi elements
//   - Gray shades for backgrounds and text
//   - Red/Yellow/Green for status indicators (latency, signal strength)
#include "thememanager.h"

QString ThemeManager::lightSheet() {
    return R"(
        QMainWindow { background: transparent; }
        #centralWidget { background: transparent; }
        #titleBar {
            background-color: #f0f0f0;
            border-top-left-radius: 12px; border-top-right-radius: 12px;
            border-bottom: 1px solid #d0d0d0;
        }
        #titleText { color: #333333; font-size: 13px; font-weight: bold; background: transparent; }
        #minBtn, #maxBtn, #closeBtn {
            background: transparent; border: none; font-size: 14px; color: #666666;
            border-radius: 4px;
        }
        #minBtn:hover, #maxBtn:hover { background-color: #e0e0e0; }
        #closeBtn:hover { background-color: #e81123; color: #ffffff; }
        #titleLabel { color: #2d2d2d; font-size: 18px; font-weight: bold; }
        #scanCard { background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 10px; }
        #scanButton {
            background-color: #34a853; color: #ffffff; border: none;
            border-radius: 8px; font-size: 14px; font-weight: bold;
        }
        #scanButton:hover { background-color: #2d9248; }
        #scanButton:pressed { background-color: #267e3e; }
        #lanList, #wifiList { background-color: transparent; border: none; font-size: 12px; }
        #lanList::item, #wifiList::item { padding: 0px; border: none; background: transparent; }
        #filterInput {
            background-color: #ffffff; border: 1px solid #e0e0e0; border-radius: 6px;
            padding: 4px 10px; font-size: 13px; color: #333333;
        }
        #filterInput:focus { border-color: #4a90d9; }
        #exportButton {
            background-color: #4a90d9; color: #ffffff; border: none;
            border-radius: 8px; font-size: 13px;
        }
        #exportButton:hover { background-color: #357abd; }
        #darkModeButton {
            background-color: #e0e0e0; color: #333333; border: none;
            border-radius: 8px; font-size: 13px;
        }
        #darkModeButton:hover { background-color: #d0d0d0; }
        #deviceMacLabel { color: #666666; font-size: 12px; font-family: 'JetBrains Mono', monospace; }
        #deviceMacLabel:hover { color: #4a90d9; }
        #statusLabel { color: #666666; font-size: 12px; }
        QStatusBar {
            background: #f0f0f0; border-top: 1px solid #d0d0d0;
            border-bottom-left-radius: 12px; border-bottom-right-radius: 12px;
            min-height: 22px;
        }
        QStatusBar::item { border: none; }
    )";
}

QString ThemeManager::darkSheet() {
    return R"(
        QMainWindow { background: transparent; }
        #centralWidget { background: transparent; }
        #titleBar {
            background-color: #2d2d2d;
            border-top-left-radius: 12px; border-top-right-radius: 12px;
            border-bottom: 1px solid #3d3d3d;
        }
        #titleText { color: #e0e0e0; font-size: 13px; font-weight: bold; }
        #minBtn, #maxBtn, #closeBtn {
            background: transparent; border: none; font-size: 14px; color: #999999;
            border-radius: 4px;
        }
        #minBtn:hover, #maxBtn:hover { background-color: #3d3d3d; }
        #closeBtn:hover { background-color: #e81123; color: #ffffff; }
        #titleLabel { color: #e0e0e0; font-size: 18px; font-weight: bold; }
        #scanCard { background-color: #2d2d2d; border: 1px solid #3d3d3d; border-radius: 10px; }
        #scanButton {
            background-color: #34a853; color: #ffffff; border: none;
            border-radius: 8px; font-size: 14px; font-weight: bold;
        }
        #scanButton:hover { background-color: #2d9248; }
        #scanButton:pressed { background-color: #267e3e; }
        #lanList, #wifiList { background-color: #252525; border: none; font-size: 12px; color: #e0e0e0; }
        #lanList::item, #wifiList::item { padding: 0px; border: none; background: transparent; }
        #filterInput {
            background-color: #3d3d3d; border: 1px solid #4d4d4d; border-radius: 6px;
            padding: 4px 10px; font-size: 13px; color: #e0e0e0;
        }
        #filterInput:focus { border-color: #4a90d9; }
        #exportButton {
            background-color: #4a90d9; color: #ffffff; border: none;
            border-radius: 8px; font-size: 13px;
        }
        #exportButton:hover { background-color: #357abd; }
        #darkModeButton {
            background-color: #555555; color: #e0e0e0; border: none;
            border-radius: 8px; font-size: 13px;
        }
        #darkModeButton:hover { background-color: #666666; }
        #deviceMacLabel { color: #999999; font-size: 12px; font-family: 'JetBrains Mono', monospace; }
        #deviceMacLabel:hover { color: #4a90d9; }
        #statusLabel { color: #999999; font-size: 12px; }
        QStatusBar {
            background: #2d2d2d; border-top: 1px solid #3d3d3d;
            border-bottom-left-radius: 12px; border-bottom-right-radius: 12px;
            min-height: 22px;
        }
        QStatusBar::item { border: none; }
        QSpinBox { background-color: #3d3d3d; color: #e0e0e0; border: 1px solid #4d4d4d; border-radius: 4px; }
        QLabel { color: #e0e0e0; }
        QLabel#intervalLabel { color: #999999; font-size: 12px; }
    )";
}

QString ThemeManager::cardStyle(bool isLan, bool isDark) {
    if (isLan)
        return isDark ? "background:#1a3a4a;border:1px solid #2a5a6a;border-radius:8px;"
                      : "background:#e3f2fd;border:1px solid #90caf9;border-radius:8px;";
    return isDark ? "background:#1a3a1a;border:1px solid #2a5a2a;border-radius:8px;"
                  : "background:#e8f5e9;border:1px solid #a5d6a7;border-radius:8px;";
}

QString ThemeManager::typeLabelStyle(bool isLan, bool isDark) {
    return QString("font-size:12px;font-weight:bold;color:%1;border:none;")
        .arg(isLan ? (isDark ? "#64b5f6" : "#4a90d9") : (isDark ? "#81c784" : "#34a853"));
}

QString ThemeManager::ssidStyle(bool isDark) {
    return QString("font-size:14px;font-weight:bold;color:%1;border:none;")
        .arg(isDark ? "#e0e0e0" : "#2d2d2d");
}

QString ThemeManager::macStyle(bool isDark) {
    return QString("font-size:12px;color:%1;font-family:'JetBrains Mono',monospace;border:none;")
        .arg(isDark ? "#999999" : "#666666");
}

QString ThemeManager::vendorStyle(bool isDark) {
    return QString("font-size:12px;color:%1;border:none;")
        .arg(isDark ? "#777777" : "#888888");
}

QString ThemeManager::signalStyle(const QString &color) {
    return QString("font-size:12px;font-weight:bold;color:%1;border:none;").arg(color);
}
