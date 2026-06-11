// speedtest.cpp - Fully async speed test on main thread
#include "speedtest.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QTcpSocket>

SpeedTest::SpeedTest(QObject *parent)
    : QObject(parent), m_latencyMs(0), m_cancelled(false)
{
    m_mgr = new QNetworkAccessManager(this);
}

void SpeedTest::cancel() {
    m_cancelled = true;
}

void SpeedTest::start() {
    m_cancelled = false;
    emit progress("测试延迟...");

    // Use TCP connect to DNS servers for latency (fast, reliable)
    struct Server { const char *host; quint16 port; };
    Server servers[] = {
        {"119.29.29.29", 53},
        {"223.5.5.5", 53},
        {"114.114.114.114", 53},
    };

    int latency = -1;
    for (const auto &s : servers) {
        QTcpSocket sock;
        QElapsedTimer timer;
        timer.start();
        sock.connectToHost(QString::fromLatin1(s.host), s.port);
        if (sock.waitForConnected(2000)) {
            latency = timer.elapsed();
            if (latency < 1) latency = 1;
            sock.disconnectFromHost();
            break;
        }
    }

    if (latency < 0) {
        emit error("无法连接到测试服务器");
        return;
    }

    m_latencyMs = latency;
    emit progress("测试下载速度...");

    // Async HTTP GET for download speed
    QNetworkRequest req(QUrl("http://speed.cloudflare.com/__down?bytes=1048576"));
    req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
    m_timer.start();
    QNetworkReply *reply = m_mgr->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onDownloadReply(reply);
    });
}

void SpeedTest::onDownloadReply(QNetworkReply *reply) {
    qint64 elapsed = m_timer.elapsed();
    qint64 bytes = reply->size();
    reply->deleteLater();

    if (m_cancelled) return;

    if (reply->error() != QNetworkReply::NoError || elapsed == 0 || bytes == 0) {
        emit error("下载测试失败");
        return;
    }

    double seconds = elapsed / 1000.0;
    double mbps = (bytes * 8.0) / seconds / (1000.0 * 1000.0);

    Result result;
    result.downloadMbps = mbps;
    result.uploadMbps = 0;
    result.latencyMs = m_latencyMs;
    result.server = "Cloudflare";

    emit resultReady(result);
}
