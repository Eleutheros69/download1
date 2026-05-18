#include "downloadworker.h"
#include <QThread>
#include <QDebug>

DownloadWorker::DownloadWorker(const QString &taskId, int blockIndex,
                               const QUrl &url, const QString &filePath,
                               qint64 start, qint64 end,
                               qint64 alreadyDownloaded,
                               int maxRetries,
                               QObject *parent)
    : QObject(parent), m_taskId(taskId), m_blockIndex(blockIndex),
    m_url(url), m_filePath(filePath), m_start(start), m_end(end),
    m_already(alreadyDownloaded), m_maxRetries(maxRetries),
    m_retryCount(0), m_paused(false), m_stopped(false), m_speedLimit(0),
    m_nam(nullptr), m_reply(nullptr), m_file(nullptr),
    m_currentPosition(start + alreadyDownloaded), m_lastWriteBytes(0), m_thisSecondBytes(0)
{
    m_speedTimer.start();
}

DownloadWorker::~DownloadWorker()
{
    if (m_reply) m_reply->deleteLater();
    if (m_nam) m_nam->deleteLater();
    if (m_file) {
        m_file->flush();
        m_file->close();
        delete m_file;
    }
}

void DownloadWorker::start()
{
    if (m_stopped) return;
    if (m_already >= (m_end - m_start + 1)) {
        emit finished(m_taskId, m_blockIndex, true, m_already);
        return;
    }
    doDownload();
}

void DownloadWorker::pause()
{
    m_paused = true;
    if (m_reply) m_reply->abort();
}

void DownloadWorker::resume()
{
    if (m_paused && !m_stopped) {
        m_paused = false;
        doDownload();
    }
}

void DownloadWorker::setSpeedLimit(int bytesPerSec)
{
    m_speedLimit = bytesPerSec;
}

void DownloadWorker::doDownload()
{
    if (!m_nam) m_nam = new QNetworkAccessManager(this);

    QNetworkRequest request(m_url);
    qint64 rangeStart = m_start + m_already;
    qint64 rangeEnd = m_end;
    QString rangeHeader = QString("bytes=%1-%2").arg(rangeStart).arg(rangeEnd);
    request.setRawHeader("Range", rangeHeader.toUtf8());
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");

    m_reply = m_nam->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, &DownloadWorker::onReadyRead);
    connect(m_reply, &QNetworkReply::finished, this, &DownloadWorker::onFinished);
    connect(m_reply, QOverload<QNetworkReply::NetworkError>::of(&QNetworkReply::errorOccurred),
            this, &DownloadWorker::onErrorOccurred);

    if (!m_file && !openFileForWrite()) {
        emit finished(m_taskId, m_blockIndex, false, m_already);
        return;
    }
    if (m_file && !m_file->seek(m_currentPosition)) {
        emit logMessage(QString("文件seek失败: %1").arg(m_filePath));
        emit finished(m_taskId, m_blockIndex, false, m_already);
        return;
    }
}

bool DownloadWorker::openFileForWrite()
{
    m_file = new QFile(m_filePath);
    if (!m_file->open(QIODevice::ReadWrite)) {
        emit logMessage(QString("无法打开文件: %1").arg(m_filePath));
        delete m_file;
        m_file = nullptr;
        return false;
    }
    return true;
}

void DownloadWorker::onReadyRead()
{
    if (m_paused || m_stopped) return;
    QByteArray data = m_reply->readAll();
    if (data.isEmpty()) return;

    // 简单限速
    if (m_speedLimit > 0) {
        m_thisSecondBytes += data.size();
        if (m_thisSecondBytes > m_speedLimit) {
            int excess = m_thisSecondBytes - m_speedLimit;
            int delayMs = static_cast<int>(excess * 1000 / m_speedLimit);
            if (delayMs > 0 && delayMs < 1000) QThread::msleep(delayMs);
            else if (delayMs >= 1000) QThread::msleep(1000);
            m_thisSecondBytes = 0;
        }
    }

    writeData(data);
    m_already += data.size();
    m_currentPosition += data.size();
    emit progress(m_taskId, m_blockIndex, m_already);
}

void DownloadWorker::writeData(const QByteArray &data)
{
    if (m_file && m_file->isOpen()) {
        qint64 written = m_file->write(data);
        if (written != data.size())
            emit logMessage("文件写入不完整");
        m_file->flush();
    }
}

void DownloadWorker::onFinished()
{
    if (m_stopped) return;
    if (m_paused) {
        m_reply->deleteLater();
        m_reply = nullptr;
        return;
    }
    bool success = false;
    if (m_reply) {
        int statusCode = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode == 206 || statusCode == 200) {
            if (m_already >= (m_end - m_start + 1)) success = true;
            else if (m_retryCount < m_maxRetries) {
                m_retryCount++;
                doDownload();
                return;
            }
        }
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file) {
        m_file->flush();
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    emit finished(m_taskId, m_blockIndex, success, m_already);
}

void DownloadWorker::onErrorOccurred(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code);
    // 错误由finished统一处理
}