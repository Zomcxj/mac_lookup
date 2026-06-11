#ifndef NETDIAG_H
#define NETDIAG_H

#include <QObject>
#include <QString>
#include <QProcess>

class NetDiag : public QObject {
    Q_OBJECT

public:
    explicit NetDiag(QObject *parent = nullptr);

    // Ping single target
    void ping(const QString &ip, int count = 4);

    // Traceroute to target
    void traceroute(const QString &ip);

    // Cancel current operation
    void cancel();

    // Get last output
    QString output() const { return m_output; }

signals:
    void lineReady(const QString &line);
    void finished(int exitCode);

private:
    QProcess *m_process;
    QString m_output;
};

#endif // NETDIAG_H
