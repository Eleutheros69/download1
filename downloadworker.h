#ifndef DOWNLOADWORKER_H
#define DOWNLOADWORKER_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QUrl>
#include <QMutex>
#include <QElapsedTimer>

class DownloadWorker : public QObject
{
    Q_OBJECT
public:
    explicit DownloadWorker(const QString &taskId, int blockIndex,
                            const QUrl &url, const QString &filePath,
                            qint64 start, qint64 end,
                            qint64 alreadyDownloaded,
                            int maxRetries,
                            QObject *parent = nullptr);
    ~DownloadWorker();

    void start();               // 开始下载
    void pause();               // 暂停
    void resume();              // 恢复（内部重新发起Range请求）
    void setSpeedLimit(int bytesPerSec);  // 限速（0表示不限速）

signals:
    void finished(const QString &taskId, int blockIndex, bool success, qint64 finalDownloaded);
    void progress(const QString &taskId, int blockIndex, qint64 bytesReceived);
    void logMessage(const QString &msg);

private slots:
    void onReadyRead();
    void onFinished();
    void onErrorOccurred(QNetworkReply::NetworkError code);

private:
    void doDownload();
    bool openFileForWrite();
    void writeData(const QByteArray &data);

    QString m_taskId;
    int m_blockIndex;
    QUrl m_url;
    QString m_filePath;
    qint64 m_start;
    qint64 m_end;
    qint64 m_already;
    int m_maxRetries;
    int m_retryCount;
    bool m_paused;
    bool m_stopped;
    int m_speedLimit;

    QNetworkAccessManager *m_nam;
    QNetworkReply *m_reply;
    QFile *m_file;
    qint64 m_currentPosition;
    QElapsedTimer m_speedTimer;
    qint64 m_lastWriteBytes;
    qint64 m_thisSecondBytes;
};

#endif // DOWNLOADWORKER_H