#include "mainwindow.h"
#include <QApplication>
#include <QIcon>
#include <QDir>
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("MAC Lookup");
    a.setApplicationVersion("1.0");

    // Register bundled fonts
    QString fontDir = QApplication::applicationDirPath() + "/data/fonts";
    QDir dir(fontDir);
    if (dir.exists()) {
        QStringList filters;
        filters << "*.ttf" << "*.otf";
        for (const QString &fontFile : dir.entryList(filters, QDir::Files)) {
            QFontDatabase::addApplicationFont(fontDir + "/" + fontFile);
        }
    }

    // Set JetBrains Mono as default application font
    QFont appFont("JetBrains Mono", 10);
    if (QFontInfo(appFont).family() == "JetBrains Mono") {
        a.setFont(appFont);
    }

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
