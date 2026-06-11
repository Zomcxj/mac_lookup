// speedtest.cpp - Network speed test implementation
#include "speedtest.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QUrl>

SpeedTest::SpeedTest(QObject *parent) : QObject(parent) {}

void SpeedTest::start() {
    emit progress("测试延迟...");

    int latency = measureLatency();
    if (latency < 0) {
        emit error("无法连接到测试服务器");
        return;
    }

    emit progress("测试下载速度...");
    double download = measureDownload();

    emit progress("测试上传速度...");
    double upload = measureUpload();

    Result result;
    result.downloadMbps = download;
    result.uploadMbps = upload;
    result.latencyMs = latency;
    result.server = "speedtest.net";

    emit resultReady(result);
}

void SpeedTest::cancel() {
    // Cancel is handled by event loop timeout
}

int SpeedTest::measureLatency() {
    QNetworkAccessManager manager;
    QEventLoop loop;
    QElapsedTimer timer;

    QNetworkRequest request(QUrl("https://www.google.com"));

    timer.start();
    QNetworkReply *reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    int latency = timer.elapsed();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
        return -1;

    return latency;
}

double SpeedTest::measureDownload() {
    QNetworkAccessManager manager;
    QEventLoop loop;
    QElapsedTimer timer;

    // Download 1MB test file
    QNetworkRequest request(QUrl("https://speed.cloudflare.com/__down?bytes=1000000"));

    timer.start();
    QNetworkReply *reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    qint64 elapsed = timer.elapsed();
    qint64 bytes = reply->bytesAvailable();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError || elapsed == 0)
        return 0.0;

    // Convert to Mbps
    double seconds = elapsed / 1000.0;
    double bits = bytes * 8.0;
    return (bits / seconds) / (1000.0 * 1000.0);
}

double SpeedTest::measureUpload() {
    QNetworkAccessManager manager;
    QEventLoop loop;
    QElapsedTimer timer;

    // Upload 500KB test data
    QByteArray data;
    data.fill('A', 500000);

    QNetworkRequest request(QUrl("https://speed.cloudflare.com/__up"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");

    timer.start();
    QNetworkReply *reply = manager.post(request, data);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    qint64 elapsed = timer.elapsed();
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError || elapsed == 0)
        return 0.0;

    // Convert to Mbps
    double seconds = elapsed / 1000.0;
    double bits = data.size() * 8.0;
    return (bits / seconds) / (1000.0 * 1000.0);
}
