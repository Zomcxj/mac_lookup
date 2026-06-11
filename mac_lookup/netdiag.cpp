// netdiag.cpp - Network diagnostics (Ping/Traceroute)
#include "netdiag.h"

NetDiag::NetDiag(QObject *parent) : QObject(parent) {
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, [this]() {
        QString data = m_process->readAllStandardOutput();
        m_output += data;
        emit lineReady(data);
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this](int code, QProcess::ExitStatus) {
        emit finished(code);
    });
}

void NetDiag::ping(const QString &ip, int count) {
    m_output.clear();
#ifdef Q_OS_WIN
    m_process->start("ping", QStringList() << "-n" << QString::number(count) << ip);
#else
    m_process->start("ping", QStringList() << "-c" << QString::number(count) << ip);
#endif
}

void NetDiag::traceroute(const QString &ip) {
    m_output.clear();
#ifdef Q_OS_WIN
    m_process->start("tracert", QStringList() << ip);
#else
    m_process->start("traceroute", QStringList() << ip);
#endif
}

void NetDiag::cancel() {
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}
