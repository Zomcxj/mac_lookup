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
#include "scanner.h"
#include "oui_database.h"

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

    QPushButton *scanButton;
    QLineEdit *filterInput;
    QSpinBox *intervalSpin;
    QLabel *statusLabel;
    QListWidget *lanList;
    QListWidget *wifiList;
    QFrame *scanCard;
    OuiDatabase ouiDb;
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
