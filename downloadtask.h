#ifndef DOWNLOADTASK_H
#define DOWNLOADTASK_H

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>
#include <QDateTime>
#include <QMutex>
#include <QElapsedTimer>

// 单个分块信息
struct BlockInfo {
    qint64 start;           // 起始字节
    qint64 end;             // 结束字节
    qint64 downloaded;      // 已下载字节
    int retryCount;         // 重试次数
    bool isFinished;        // 是否完成
};

// 任务状态
enum class TaskStatus {
    Waiting,        // 等待调度
    Downloading,    // 下载中
    Paused,         // 已暂停
    Completed,      // 已完成
    Error           // 错误
};

// 任务信息，可拷贝用于UI展示
struct TaskInfo {
    QString id;                     // UUID
    QUrl url;
    QString savePath;               // 完整保存路径（含文件名）
    QString fileName;               // 文件名
    qint64 totalSize;               // 总大小（0表示未知）
    qint64 downloadedSize;          // 已下载总大小
    int threadCount;                // 分块线程数
    int maxRetries;                 // 每个分块最大重试次数
    TaskStatus status;
    QDateTime createTime;
    QVector<BlockInfo> blocks;      // 分块列表
};

// 用于在管理器和界面间传递的元数据，避免拷贝大块数据
class DownloadTask : public QObject
{
    Q_OBJECT
public:
    explicit DownloadTask(const TaskInfo &info, QObject *parent = nullptr);
    ~DownloadTask();

    TaskInfo getInfo() const { return m_info; }
    void updateInfo(const TaskInfo &info) { m_info = info; }

    // 线程安全地更新下载进度（由Worker调用）
    void addDownloadedBytes(qint64 bytes);

    // 获取任务总进度（0-100）
    int progress() const;

signals:
    void progressUpdated(int percent);      // 进度变化信号（UI绑定）
    void speedUpdated(qint64 speed);        // 瞬时速度（字节/秒）
    void statusChanged(TaskStatus status);
    void blockFinished(int blockIndex);     // 某个分块完成

private:
    TaskInfo m_info;
    mutable QMutex m_mutex;
    qint64 m_lastDownloaded;
    QElapsedTimer m_speedTimer;
};

#endif // DOWNLOADTASK_H