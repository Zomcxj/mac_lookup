// mainwindow.h - 统一版（局域网 + WiFi，跨平台）
#pragma once
#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QHash>
#include <QSet>
#include <QFile>
#include <QListWidget>
#include <QFrame>
#include <QTimer>
#include <QLineEdit>
#include <QSpinBox>
#include <QtConcurrent>
#include <QMouseEvent>

#ifdef Q_OS_WIN
#include <windows.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#include <wlanapi.h>
#endif

struct CompanyInfo {
    QString country;
    QString companyName;
};

struct NetworkDevice {
    QString type;        // "LAN" 或 "WiFi"
    QString ip;          // LAN: IP地址, WiFi: SSID
    QString mac;         // MAC地址
    int latency;         // LAN: 延迟ms, WiFi: 0
    int signal;          // WiFi: 信号强度dBm, LAN: 0
    QString manufacturer;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private slots:
    void loadData(const QString &fileName);
    void startScan();
    void doScan();
    void onScanFinished(const QList<NetworkDevice> &devices);
    void filterDevices(const QString &text);
    void exportResults();
    void scanPorts(const QString &ip);
    void toggleDarkMode();
    void viewDeviceHistory(const QString &key);
    void showTopology();

private:
    void setupUI();
    void applyStyles();
    void onListContextMenu(const QPoint &pos);
    void copySelected();
    QWidget* createDeviceCard(const NetworkDevice &device);
    void copyDeviceMac();
    void lookupManufacturer(NetworkDevice &dev) const;

    QList<NetworkDevice> arpScan();
    QList<NetworkDevice> wifiScan();
    QString getAdapterMac();
#ifdef Q_OS_WIN
    QList<QString> getSubnetsToScan();
#endif

    QPushButton *scanButton;
    QLineEdit *filterInput;
    QSpinBox *intervalSpin;
    QLabel *statusLabel;
    QListWidget *lanList;
    QListWidget *wifiList;
    QFrame *scanCard;
    QHash<QString, CompanyInfo> companyMap;
    QTimer *scanTimer;
    bool isScanning;
    bool scanInProgress;
    QLabel *deviceMacLabel;
    QSet<QString> discoveredDevices;
    QHash<QString, QStringList> deviceHistory;
    QString m_deviceMac;
    bool darkMode;
    bool dragging;
    QPoint dragPos;
};
