#include "downloadmanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

DownloadManager* DownloadManager::m_instance = nullptr;

DownloadManager* DownloadManager::instance()
{
    if (!m_instance) {
        m_instance = new DownloadManager(qApp);
    }
    return m_instance;
}

DownloadManager::DownloadManager(QObject *parent)
    : QObject(parent), m_maxConcurrent(3), m_globalSpeedLimit(0)
{
    loadTasksFromDisk();
    for (auto task : m_tasks) {
        TaskInfo info = task->getInfo();
        if (info.status == TaskStatus::Downloading || info.status == TaskStatus::Waiting) {
            if (info.status == TaskStatus::Downloading) {
                info.status = TaskStatus::Paused;
                task->updateInfo(info);
            }
            m_waitingQueue.enqueue(info.id);
        }
    }
    scheduleTasks();
}

DownloadManager::~DownloadManager()
{
    saveTasksToDisk();
    for (auto it = m_taskThreads.begin(); it != m_taskThreads.end(); ++it) {
        for (QThread *thread : it.value()) {
            thread->quit();
            thread->wait(1000);
            delete thread;
        }
    }
}

void DownloadManager::addTask(const TaskInfo &info)
{
    if (m_tasks.contains(info.id)) return;
    DownloadTask *task = new DownloadTask(info, this);
    m_tasks[info.id] = task;
    m_waitingQueue.enqueue(info.id);
    emit taskAdded(info);
    scheduleTasks();
    saveTasksToDisk();
}

void DownloadManager::pauseTask(const QString &taskId)
{
    if (!m_tasks.contains(taskId)) return;
    TaskInfo info = m_tasks[taskId]->getInfo();
    if (info.status == TaskStatus::Downloading) {
        info.status = TaskStatus::Paused;
        m_tasks[taskId]->updateInfo(info);
        stopAllWorkersForTask(taskId);
        emit taskStatusChanged(taskId, TaskStatus::Paused);
        saveTasksToDisk();
    }
}

void DownloadManager::resumeTask(const QString &taskId)
{
    if (!m_tasks.contains(taskId)) return;
    TaskInfo info = m_tasks[taskId]->getInfo();
    if (info.status == TaskStatus::Paused) {
        info.status = TaskStatus::Waiting;
        m_tasks[taskId]->updateInfo(info);
        m_waitingQueue.enqueue(taskId);
        emit taskStatusChanged(taskId, TaskStatus::Waiting);
        scheduleTasks();
        saveTasksToDisk();
    }
}

void DownloadManager::cancelTask(const QString &taskId, bool deleteFile)
{
    if (!m_tasks.contains(taskId)) return;
    pauseTask(taskId);
    TaskInfo info = m_tasks[taskId]->getInfo();
    if (deleteFile && !info.savePath.isEmpty())
        QFile::remove(info.savePath);
    m_tasks.remove(taskId);
    m_waitingQueue.removeAll(taskId);
    emit taskFinished(taskId, false);
    saveTasksToDisk();
}

void DownloadManager::setMaxConcurrentTasks(int max)
{
    m_maxConcurrent = max;
    scheduleTasks();
}

void DownloadManager::setGlobalSpeedLimit(int bytesPerSec)
{
    m_globalSpeedLimit = bytesPerSec;
    int activeWorkerCount = 0;
    for (auto workers : m_taskWorkers) activeWorkerCount += workers.size();
    int perWorkerLimit = (m_globalSpeedLimit > 0 && activeWorkerCount > 0) ? m_globalSpeedLimit / activeWorkerCount : 0;
    for (auto workers : m_taskWorkers)
        for (DownloadWorker *worker : workers)
            worker->setSpeedLimit(perWorkerLimit);
}

QList<TaskInfo> DownloadManager::getAllTasks() const
{
    QList<TaskInfo> list;
    for (auto task : m_tasks) list.append(task->getInfo());
    return list;
}

void DownloadManager::onWorkerFinished(const QString &taskId, int blockIndex, bool success, qint64 downloaded)
{
    if (!m_tasks.contains(taskId)) return;
    TaskInfo info = m_tasks[taskId]->getInfo();
    if (blockIndex >= 0 && blockIndex < info.blocks.size()) {
        info.blocks[blockIndex].downloaded = downloaded;
        info.blocks[blockIndex].isFinished = success;
        m_tasks[taskId]->updateInfo(info);
    }
    qint64 totalDownloaded = 0;
    for (auto &block : info.blocks) totalDownloaded += block.downloaded;
    int percent = (info.totalSize > 0) ? (totalDownloaded * 100 / info.totalSize) : 0;
    emit taskProgress(taskId, percent, 0);

    bool allFinished = true;
    for (auto &block : info.blocks) if (!block.isFinished) { allFinished = false; break; }
    if (allFinished) {
        info.status = TaskStatus::Completed;
        m_tasks[taskId]->updateInfo(info);
        emit taskStatusChanged(taskId, TaskStatus::Completed);
        emit taskFinished(taskId, true);
        stopAllWorkersForTask(taskId);
        saveTasksToDisk();
    } else if (!success) {
        QTimer::singleShot(2000, this, [this, taskId, blockIndex](){
            if (m_tasks.contains(taskId)) startTask(m_tasks[taskId]);
        });
    }
}

void DownloadManager::onWorkerProgress(const QString &taskId, int blockIndex, qint64 received)
{
    Q_UNUSED(blockIndex);
    updateTaskProgress(taskId);
}

void DownloadManager::scheduleTasks()
{
    int downloadingCount = 0;
    for (auto task : m_tasks)
        if (task->getInfo().status == TaskStatus::Downloading)
            downloadingCount++;
    while (downloadingCount < m_maxConcurrent && !m_waitingQueue.isEmpty()) {
        QString taskId = m_waitingQueue.dequeue();
        DownloadTask *task = m_tasks.value(taskId);
        if (!task) continue;
        TaskInfo info = task->getInfo();
        if (info.status == TaskStatus::Waiting || info.status == TaskStatus::Paused) {
            info.status = TaskStatus::Downloading;
            task->updateInfo(info);
            emit taskStatusChanged(taskId, TaskStatus::Downloading);
            startTask(task);
            downloadingCount++;
        }
    }
    saveTasksToDisk();
}

void DownloadManager::startTask(DownloadTask *task)
{
    TaskInfo info = task->getInfo();
    if (info.totalSize == 0) {
        QNetworkAccessManager nam;
        QNetworkRequest request(info.url);
        request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
        QNetworkReply *reply = nam.head(request);
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        if (reply->error() == QNetworkReply::NoError) {
            qint64 size = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
            if (size > 0) info.totalSize = size;
            QString acceptRanges = reply->rawHeader("Accept-Ranges");
            if (acceptRanges.toLower() != "bytes") info.threadCount = 1;
        } else {
            info.threadCount = 1;
        }
        reply->deleteLater();
        task->updateInfo(info);
    }
    if (info.blocks.isEmpty()) {
        qint64 blockSize = (info.totalSize + info.threadCount - 1) / info.threadCount;
        qint64 start = 0;
        for (int i = 0; i < info.threadCount; ++i) {
            BlockInfo block;
            block.start = start;
            block.end = (i == info.threadCount - 1) ? info.totalSize - 1 : start + blockSize - 1;
            block.downloaded = 0;
            block.isFinished = false;
            block.retryCount = 0;
            info.blocks.append(block);
            start = block.end + 1;
        }
        task->updateInfo(info);
    }
    QList<QThread*> threads;
    QList<DownloadWorker*> workers;
    for (int i = 0; i < info.blocks.size(); ++i) {
        BlockInfo &block = info.blocks[i];
        if (block.isFinished) continue;
        QThread *thread = new QThread(this);
        DownloadWorker *worker = new DownloadWorker(info.id, i, info.url, info.savePath,
                                                    block.start, block.end,
                                                    block.downloaded, info.maxRetries);
        worker->moveToThread(thread);
        connect(thread, &QThread::started, worker, &DownloadWorker::start);
        connect(worker, &DownloadWorker::finished, this, &DownloadManager::onWorkerFinished);
        connect(worker, &DownloadWorker::progress, this, &DownloadManager::onWorkerProgress);
        connect(worker, &DownloadWorker::logMessage, this, &DownloadManager::logMessage);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);
        threads.append(thread);
        workers.append(worker);
        thread->start();
    }
    m_taskThreads[info.id] = threads;
    m_taskWorkers[info.id] = workers;
}

void DownloadManager::stopAllWorkersForTask(const QString &taskId)
{
    if (m_taskThreads.contains(taskId)) {
        for (QThread *thread : m_taskThreads[taskId]) {
            thread->quit();
            thread->wait(1000);
            delete thread;
        }
        m_taskThreads.remove(taskId);
        m_taskWorkers.remove(taskId);
    }
}

void DownloadManager::updateTaskProgress(const QString &taskId)
{
    if (!m_tasks.contains(taskId)) return;
    DownloadTask *task = m_tasks[taskId];
    TaskInfo info = task->getInfo();
    qint64 totalDownloaded = 0;
    for (auto &block : info.blocks) totalDownloaded += block.downloaded;
    int percent = (info.totalSize > 0) ? (totalDownloaded * 100 / info.totalSize) : 0;
    static QHash<QString, qint64> lastBytes, lastTime;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 speed = 0;
    if (lastBytes.contains(taskId) && lastTime.contains(taskId)) {
        qint64 deltaBytes = totalDownloaded - lastBytes[taskId];
        qint64 deltaTime = now - lastTime[taskId];
        if (deltaTime > 0) speed = deltaBytes * 1000 / deltaTime;
    }
    lastBytes[taskId] = totalDownloaded;
    lastTime[taskId] = now;
    emit taskProgress(taskId, percent, speed);
}

void DownloadManager::saveTasksToDisk()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(configPath);
    QFile file(configPath + "/tasks.json");
    if (!file.open(QIODevice::WriteOnly)) return;
    QJsonArray tasksArray;
    for (auto task : m_tasks) {
        TaskInfo info = task->getInfo();
        QJsonObject obj;
        obj["id"] = info.id;
        obj["url"] = info.url.toString();
        obj["savePath"] = info.savePath;
        obj["fileName"] = info.fileName;
        obj["totalSize"] = QString::number(info.totalSize);
        obj["downloadedSize"] = QString::number(info.downloadedSize);
        obj["threadCount"] = info.threadCount;
        obj["maxRetries"] = info.maxRetries;
        obj["status"] = static_cast<int>(info.status);
        QJsonArray blocksArr;
        for (auto &block : info.blocks) {
            QJsonObject b;
            b["start"] = QString::number(block.start);
            b["end"] = QString::number(block.end);
            b["downloaded"] = QString::number(block.downloaded);
            b["isFinished"] = block.isFinished;
            blocksArr.append(b);
        }
        obj["blocks"] = blocksArr;
        tasksArray.append(obj);
    }
    QJsonDocument doc(tasksArray);
    file.write(doc.toJson());
}

void DownloadManager::loadTasksFromDisk()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile file(configPath + "/tasks.json");
    if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray tasksArray = doc.array();
    for (const auto &val : tasksArray) {
        QJsonObject obj = val.toObject();
        TaskInfo info;
        info.id = obj["id"].toString();
        info.url = QUrl(obj["url"].toString());
        info.savePath = obj["savePath"].toString();
        info.fileName = obj["fileName"].toString();
        info.totalSize = obj["totalSize"].toString().toLongLong();
        info.downloadedSize = obj["downloadedSize"].toString().toLongLong();
        info.threadCount = obj["threadCount"].toInt();
        info.maxRetries = obj["maxRetries"].toInt();
        info.status = static_cast<TaskStatus>(obj["status"].toInt());
        QJsonArray blocksArr = obj["blocks"].toArray();
        for (const auto &bval : blocksArr) {
            QJsonObject b = bval.toObject();
            BlockInfo block;
            block.start = b["start"].toString().toLongLong();
            block.end = b["end"].toString().toLongLong();
            block.downloaded = b["downloaded"].toString().toLongLong();
            block.isFinished = b["isFinished"].toBool();
            info.blocks.append(block);
        }
        DownloadTask *task = new DownloadTask(info, this);
        m_tasks[info.id] = task;
    }
}