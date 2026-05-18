#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QTimer>
#include <QUuid>
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupUI();

    auto *mgr = DownloadManager::instance();
    connect(mgr, &DownloadManager::taskAdded, this, &MainWindow::onTaskAdded);
    connect(mgr, &DownloadManager::taskProgress, this, &MainWindow::onTaskProgress);
    connect(mgr, &DownloadManager::taskStatusChanged, this, &MainWindow::onTaskStatusChanged);
    connect(mgr, &DownloadManager::taskFinished, this, [this](const QString &taskId, bool success){
        if (success) QMessageBox::information(this, "完成", "任务 " + taskId + " 下载完成");
        updateGlobalStats();
    });
    connect(mgr, &DownloadManager::logMessage, this, [](const QString &msg){ /* 可选日志 */ });

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateGlobalStats);
    timer->start(1000);
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
    setWindowTitle("多线程高速下载工具");
    resize(1000, 600);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // 工具栏
    QHBoxLayout *toolBar = new QHBoxLayout();
    m_newBtn = new QPushButton("新建下载");
    m_startBtn = new QPushButton("开始");
    m_pauseBtn = new QPushButton("暂停");
    m_resumeBtn = new QPushButton("继续");
    m_cancelBtn = new QPushButton("取消删除");
    m_openDirBtn = new QPushButton("打开目录");
    toolBar->addWidget(m_newBtn);
    toolBar->addWidget(m_startBtn);
    toolBar->addWidget(m_pauseBtn);
    toolBar->addWidget(m_resumeBtn);
    toolBar->addWidget(m_cancelBtn);
    toolBar->addWidget(m_openDirBtn);
    toolBar->addStretch();
    toolBar->addWidget(new QLabel("全局限速(KB/s):"));
    m_speedCombo = new QComboBox();
    m_speedCombo->addItems({"不限速", "100", "500", "1024", "2048", "5120"});
    toolBar->addWidget(m_speedCombo);
    toolBar->addWidget(new QLabel("并行任务数:"));
    m_concurrentSpin = new QSpinBox();
    m_concurrentSpin->setRange(1, 10);
    m_concurrentSpin->setValue(3);
    toolBar->addWidget(m_concurrentSpin);
    mainLayout->addLayout(toolBar);

    // 任务表格
    m_taskTable = new QTableWidget(0, 11);
    QStringList headers;
    headers << "ID" << "文件名" << "进度" << "速度" << "已下载" << "总大小" << "剩余时间" << "线程数" << "状态" << "操作" << "保存路径";
    m_taskTable->setHorizontalHeaderLabels(headers);
    m_taskTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_taskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_taskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_taskTable);

    // 全局状态栏
    QHBoxLayout *statusBar = new QHBoxLayout();
    m_globalSpeedLabel = new QLabel("总速度: 0 KB/s");
    m_globalProgressLabel = new QLabel("总进度: 0%");
    statusBar->addWidget(m_globalSpeedLabel);
    statusBar->addStretch();
    statusBar->addWidget(m_globalProgressLabel);
    mainLayout->addLayout(statusBar);

    // 连接信号
    connect(m_newBtn, &QPushButton::clicked, this, &MainWindow::onNewDownload);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(m_pauseBtn, &QPushButton::clicked, this, &MainWindow::onPause);
    connect(m_resumeBtn, &QPushButton::clicked, this, &MainWindow::onResume);
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancel);
    connect(m_openDirBtn, &QPushButton::clicked, this, &MainWindow::onOpenFolder);
    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onGlobalSpeedChanged);
    connect(m_concurrentSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onMaxConcurrentChanged);
    connect(m_taskTable, &QTableWidget::itemSelectionChanged, [this](){
        int row = m_taskTable->currentRow();
        if (row >= 0) m_selectedTaskId = m_taskTable->item(row, 0)->text();
        else m_selectedTaskId.clear();
    });
}

void MainWindow::onNewDownload()
{
    bool ok;
    QString urlStr = QInputDialog::getText(this, "新建下载", "请输入下载链接:", QLineEdit::Normal, "", &ok);
    if (!ok || urlStr.isEmpty()) return;
    QUrl url(urlStr);
    if (!url.isValid()) {
        QMessageBox::warning(this, "错误", "无效的URL");
        return;
    }
    QString saveDir = QFileDialog::getExistingDirectory(this, "选择保存目录", QDir::homePath());
    if (saveDir.isEmpty()) return;
    QString fileName = QFileInfo(url.path()).fileName();
    if (fileName.isEmpty()) fileName = "downloaded_file";
    QString savePath = QDir(saveDir).filePath(fileName);
    int threads = QInputDialog::getInt(this, "分块线程数", "线程数(1-32):", 8, 1, 32);
    TaskInfo info;
    info.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    info.url = url;
    info.savePath = savePath;
    info.fileName = fileName;
    info.totalSize = 0;
    info.downloadedSize = 0;
    info.threadCount = threads;
    info.maxRetries = 3;
    info.status = TaskStatus::Waiting;
    DownloadManager::instance()->addTask(info);
}

void MainWindow::onStart()
{
    if (!m_selectedTaskId.isEmpty())
        DownloadManager::instance()->resumeTask(m_selectedTaskId);
}

void MainWindow::onPause()
{
    if (!m_selectedTaskId.isEmpty())
        DownloadManager::instance()->pauseTask(m_selectedTaskId);
}

void MainWindow::onResume()
{
    onStart();
}

void MainWindow::onCancel()
{
    if (!m_selectedTaskId.isEmpty()) {
        bool delFile = QMessageBox::question(this, "删除", "是否同时删除已下载的临时文件？") == QMessageBox::Yes;
        DownloadManager::instance()->cancelTask(m_selectedTaskId, delFile);
        int row = findRowByTaskId(m_selectedTaskId);
        if (row >= 0) m_taskTable->removeRow(row);
        m_taskRowMap.remove(m_selectedTaskId);
        m_selectedTaskId.clear();
        updateGlobalStats();
    }
}

void MainWindow::onOpenFolder()
{
    if (!m_selectedTaskId.isEmpty()) {
        TaskInfo info;
        for (auto t : DownloadManager::instance()->getAllTasks()) {
            if (t.id == m_selectedTaskId) {
                info = t;
                break;
            }
        }
        if (!info.savePath.isEmpty()) {
            QDir dir = QFileInfo(info.savePath).dir();
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
        }
    }
}

void MainWindow::onGlobalSpeedChanged(int idx)
{
    int speedKB = 0;
    if (idx > 0) speedKB = m_speedCombo->currentText().toInt();
    DownloadManager::instance()->setGlobalSpeedLimit(speedKB * 1024);
}

void MainWindow::onMaxConcurrentChanged(int value)
{
    DownloadManager::instance()->setMaxConcurrentTasks(value);
}

void MainWindow::onTaskAdded(const TaskInfo &info)
{
    addTaskToTable(info);
}

void MainWindow::addTaskToTable(const TaskInfo &info)
{
    int row = m_taskTable->rowCount();
    m_taskTable->insertRow(row);
    m_taskTable->setItem(row, 0, new QTableWidgetItem(info.id));
    m_taskTable->setItem(row, 1, new QTableWidgetItem(info.fileName));
    QProgressBar *bar = new QProgressBar();
    bar->setValue(0);
    m_taskTable->setCellWidget(row, 2, bar);
    m_taskTable->setItem(row, 3, new QTableWidgetItem("0 KB/s"));
    m_taskTable->setItem(row, 4, new QTableWidgetItem(formatSize(0)));
    m_taskTable->setItem(row, 5, new QTableWidgetItem(formatSize(info.totalSize)));
    m_taskTable->setItem(row, 6, new QTableWidgetItem("--"));
    m_taskTable->setItem(row, 7, new QTableWidgetItem(QString::number(info.threadCount)));
    m_taskTable->setItem(row, 8, new QTableWidgetItem(statusToString(info.status)));
    m_taskTable->setItem(row, 9, new QTableWidgetItem(""));
    m_taskTable->setItem(row, 10, new QTableWidgetItem(info.savePath));
    m_taskRowMap[info.id] = row;
}

void MainWindow::onTaskProgress(const QString &taskId, int percent, qint64 speed)
{
    int row = findRowByTaskId(taskId);
    if (row < 0) return;
    QProgressBar *bar = qobject_cast<QProgressBar*>(m_taskTable->cellWidget(row, 2));
    if (bar) bar->setValue(percent);
    m_taskTable->item(row, 3)->setText(QString("%1 KB/s").arg(speed/1024));
    for (auto t : DownloadManager::instance()->getAllTasks()) {
        if (t.id == taskId) {
            m_taskTable->item(row, 4)->setText(formatSize(t.downloadedSize));
            if (speed > 0 && t.totalSize > t.downloadedSize) {
                int remainSec = (t.totalSize - t.downloadedSize) / speed;
                m_taskTable->item(row, 6)->setText(QString("%1 s").arg(remainSec));
            } else {
                m_taskTable->item(row, 6)->setText("--");
            }
            break;
        }
    }
}

void MainWindow::onTaskStatusChanged(const QString &taskId, TaskStatus status)
{
    int row = findRowByTaskId(taskId);
    if (row >= 0) m_taskTable->item(row, 8)->setText(statusToString(status));
    updateGlobalStats();
}

void MainWindow::updateGlobalStats()
{
    qint64 totalDownloaded = 0, totalSize = 0;
    for (auto t : DownloadManager::instance()->getAllTasks()) {
        totalDownloaded += t.downloadedSize;
        totalSize += t.totalSize;
    }
    int percent = totalSize > 0 ? (totalDownloaded * 100 / totalSize) : 0;
    m_globalProgressLabel->setText(QString("总进度: %1%").arg(percent));
}

int MainWindow::findRowByTaskId(const QString &taskId)
{
    return m_taskRowMap.value(taskId, -1);
}

QString MainWindow::formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024*1024) return QString("%1 KB").arg(bytes/1024);
    if (bytes < 1024*1024*1024) return QString("%1 MB").arg(bytes/1024/1024);
    return QString("%1 GB").arg(bytes/1024/1024/1024);
}

QString MainWindow::statusToString(TaskStatus status)
{
    switch (status) {
    case TaskStatus::Waiting: return "等待";
    case TaskStatus::Downloading: return "下载中";
    case TaskStatus::Paused: return "暂停";
    case TaskStatus::Completed: return "完成";
    case TaskStatus::Error: return "错误";
    default: return "未知";
    }
}