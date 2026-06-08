#include "mainwindow.h"
#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("MAC Lookup");
    a.setApplicationVersion("1.0");

#ifdef Q_OS_WIN
    // Windows: 使用资源图标
#else
    a.setWindowIcon(QIcon(":/icons/mac_lookup.png"));
#endif

    MainWindow w;
#ifndef Q_OS_WIN
    w.setWindowIcon(QIcon(":/icons/mac_lookup.png"));
#endif
    w.show();

    return a.exec();
}
