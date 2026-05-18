#include "downloadtask.h"
#include <QUuid>

DownloadTask::DownloadTask(const TaskInfo &info, QObject *parent)
    : QObject(parent), m_info(info), m_lastDownloaded(0)
{
    if (m_info.id.isEmpty())
        m_info.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_speedTimer.start();
}

DownloadTask::~DownloadTask() {}

void DownloadTask::addDownloadedBytes(qint64 bytes)
{
    QMutexLocker locker(&m_mutex);
    m_info.downloadedSize += bytes;
}

int DownloadTask::progress() const
{
    QMutexLocker locker(&m_mutex);
    if (m_info.totalSize <= 0) return 0;
    return static_cast<int>(m_info.downloadedSize * 100 / m_info.totalSize);
}