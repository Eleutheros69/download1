#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QThread>
#include <QMutex>
#include "downloadtask.h"
#include "downloadworker.h"

class DownloadManager : public QObject
{
    Q_OBJECT
public:
    static DownloadManager* instance();
    ~DownloadManager();

    void addTask(const TaskInfo &info);
    void pauseTask(const QString &taskId);
    void resumeTask(const QString &taskId);
    void cancelTask(const QString &taskId, bool deleteFile = false);
    void setMaxConcurrentTasks(int max);
    void setGlobalSpeedLimit(int bytesPerSec);

    QList<TaskInfo> getAllTasks() const;

signals:
    void taskAdded(const TaskInfo &info);
    void taskProgress(const QString &taskId, int percent, qint64 speed);
    void taskStatusChanged(const QString &taskId, TaskStatus status);
    void taskFinished(const QString &taskId, bool success);
    void logMessage(const QString &msg);

private slots:
    void onWorkerFinished(const QString &taskId, int blockIndex, bool success, qint64 downloaded);
    void onWorkerProgress(const QString &taskId, int blockIndex, qint64 received);
    void scheduleTasks();

private:
    explicit DownloadManager(QObject *parent = nullptr);
    void saveTasksToDisk();
    void loadTasksFromDisk();
    void startTask(DownloadTask *task);
    void stopAllWorkersForTask(const QString &taskId);
    void updateTaskProgress(const QString &taskId);

    QHash<QString, DownloadTask*> m_tasks;
    QHash<QString, QList<QThread*>> m_taskThreads;
    QHash<QString, QList<DownloadWorker*>> m_taskWorkers;
    QQueue<QString> m_waitingQueue;
    int m_maxConcurrent;
    int m_globalSpeedLimit;
    QMutex m_mutex;
    static DownloadManager *m_instance;
};

#endif // DOWNLOADMANAGER_H