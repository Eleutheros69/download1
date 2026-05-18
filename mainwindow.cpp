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
#include <QStandardPaths>
#include <QElapsedTimer>
#include <QApplication>
#include <QPalette>

// ==================== 全局样式表（增强对话框支持） ====================
static const QString STYLE_SHEET = R"(
    QMainWindow {
        background-color: #f0f2f5;
    }
    QWidget {
        background-color: #f0f2f5;
        color: #212529;
        font-family: "Segoe UI", "Microsoft YaHei", "PingFang SC", "Helvetica Neue", Arial, sans-serif;
        font-size: 9pt;
    }
    QTableWidget {
        background-color: white;
        alternate-background-color: #f8f9fa;
        gridline-color: #dee2e6;
        selection-background-color: #4CAF50;
        selection-color: white;
    }
    QTableWidget::item {
        padding: 6px;
    }
    QHeaderView::section {
        background-color: #e9ecef;
        padding: 8px;
        border: none;
        font-weight: 600;
    }
    QPushButton {
        background-color: #4CAF50;
        color: white;
        border: none;
        padding: 6px 14px;
        border-radius: 4px;
        font-weight: 500;
    }
    QPushButton:hover {
        background-color: #45a049;
    }
    QPushButton:pressed {
        background-color: #3d8b40;
    }
    QPushButton#cancelBtn {
        background-color: #dc3545;
    }
    QPushButton#cancelBtn:hover {
        background-color: #c82333;
    }
    QPushButton#openBtn {
        background-color: #17a2b8;
    }
    QPushButton#openBtn:hover {
        background-color: #138496;
    }
    QProgressBar {
        border: 1px solid #ced4da;
        border-radius: 3px;
        text-align: center;
        background-color: #e9ecef;
        height: 18px;
    }
    QProgressBar::chunk {
        background-color: #4CAF50;
        border-radius: 3px;
    }
    QComboBox, QSpinBox {
        background-color: white;
        color: #212529;
        padding: 4px 6px;
        border: 1px solid #ced4da;
        border-radius: 4px;
    }
    QComboBox:hover, QSpinBox:hover {
        border-color: #4CAF50;
    }
    QComboBox::drop-down {
        border: none;
    }
    QComboBox QAbstractItemView {
        background-color: white;
        color: #212529;
        selection-background-color: #4CAF50;
        selection-color: white;
    }
    QDialog {
        background-color: #f0f2f5;
    }
    QMessageBox {
        background-color: #f0f2f5;
    }
    QMessageBox QLabel {
        color: #212529;
    }
    QInputDialog {
        background-color: #f0f2f5;
    }
    QInputDialog QLabel {
        color: #212529;
    }
    QInputDialog QLineEdit {
        background-color: white;
        color: #212529;
        border: 1px solid #ced4da;
        border-radius: 4px;
        padding: 4px;
    }
    QLabel {
        color: #212529;
    }
)";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_lastTotalDownloaded(0)
{
    // 设置全局调色板，增强对话框对比度
    QPalette pal = QApplication::palette();
    pal.setColor(QPalette::Window, QColor(240,242,245));
    pal.setColor(QPalette::WindowText, QColor(33,37,41));
    pal.setColor(QPalette::Base, QColor(255,255,255));
    pal.setColor(QPalette::Text, QColor(33,37,41));
    pal.setColor(QPalette::Button, QColor(76,175,80));
    pal.setColor(QPalette::ButtonText, Qt::white);
    QApplication::setPalette(pal);

    setupUI();

    auto *mgr = DownloadManager::instance();
    connect(mgr, &DownloadManager::taskAdded, this, &MainWindow::onTaskAdded);
    connect(mgr, &DownloadManager::taskProgress, this, &MainWindow::onTaskProgress);
    connect(mgr, &DownloadManager::taskStatusChanged, this, &MainWindow::onTaskStatusChanged);
    connect(mgr, &DownloadManager::taskFinished, this, [this](const QString &taskId, bool success){
        if (success) QMessageBox::information(this, "完成", "任务 " + taskId.left(8) + " 下载完成");
        updateGlobalStats();
    });
    connect(mgr, &DownloadManager::logMessage, this, [](const QString &msg){
        qDebug() << "[DownloadManager]" << msg;
    });

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateGlobalStats);
    timer->start(1000);
    m_globalSpeedTimer.start();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
    setWindowTitle("多线程高速下载工具");
    resize(1200, 700);
    setMinimumSize(900, 500);
    setStyleSheet(STYLE_SHEET);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    // 工具栏
    QHBoxLayout *toolBar = new QHBoxLayout();
    toolBar->setSpacing(8);
    m_newBtn = new QPushButton("新建下载");
    m_startBtn = new QPushButton("开始");
    m_pauseBtn = new QPushButton("暂停");
    m_resumeBtn = new QPushButton("继续");
    m_cancelBtn = new QPushButton("取消删除");
    m_openDirBtn = new QPushButton("打开目录");
    m_cancelBtn->setObjectName("cancelBtn");
    m_openDirBtn->setObjectName("openBtn");
    toolBar->addWidget(m_newBtn);
    toolBar->addWidget(m_startBtn);
    toolBar->addWidget(m_pauseBtn);
    toolBar->addWidget(m_resumeBtn);
    toolBar->addWidget(m_cancelBtn);
    toolBar->addWidget(m_openDirBtn);
    toolBar->addStretch();
    toolBar->addWidget(new QLabel("全局限速:"));
    m_speedCombo = new QComboBox();
    // 存储实际限速值（KB/s）
    m_speedCombo->addItem("不限速", 0);
    m_speedCombo->addItem("100 KB/s", 100);
    m_speedCombo->addItem("500 KB/s", 500);
    m_speedCombo->addItem("1 MB/s", 1024);
    m_speedCombo->addItem("2 MB/s", 2048);
    m_speedCombo->addItem("5 MB/s", 5120);
    m_speedCombo->addItem("10 MB/s", 10240);
    m_speedCombo->setCurrentIndex(0);
    m_speedCombo->setMinimumWidth(110);
    toolBar->addWidget(m_speedCombo);
    toolBar->addWidget(new QLabel("并行任务:"));
    m_concurrentSpin = new QSpinBox();
    m_concurrentSpin->setRange(1, 10);
    m_concurrentSpin->setValue(3);
    m_concurrentSpin->setMinimumWidth(65);
    toolBar->addWidget(m_concurrentSpin);
    mainLayout->addLayout(toolBar);

    // 任务表格
    m_taskTable = new QTableWidget(0, 10);
    m_taskTable->setAlternatingRowColors(true);
    m_taskTable->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_taskTable->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    QStringList headers;
    headers << "文件名" << "进度" << "速度" << "已下载" << "总大小"
            << "剩余时间" << "线程数" << "状态" << "保存路径" << "任务ID(隐藏)";
    m_taskTable->setHorizontalHeaderLabels(headers);

    // 设置列宽
    m_taskTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_taskTable->setColumnWidth(1, 100);
    m_taskTable->setColumnWidth(2, 90);
    m_taskTable->setColumnWidth(3, 100);
    m_taskTable->setColumnWidth(4, 100);
    m_taskTable->setColumnWidth(5, 80);
    m_taskTable->setColumnWidth(6, 60);
    m_taskTable->setColumnWidth(7, 70);
    m_taskTable->horizontalHeader()->setSectionResizeMode(8, QHeaderView::Interactive);
    m_taskTable->setColumnWidth(9, 0);  // 隐藏ID列

    m_taskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_taskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_taskTable);

    // 全局状态栏
    QHBoxLayout *statusBar = new QHBoxLayout();
    m_globalSpeedLabel = new QLabel("总下载速度: 0 KB/s");
    m_globalProgressLabel = new QLabel("总进度: 0%");
    statusBar->addWidget(m_globalSpeedLabel);
    statusBar->addStretch();
    statusBar->addWidget(m_globalProgressLabel);
    mainLayout->addLayout(statusBar);

    // 信号连接
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
        if (row >= 0) {
            m_selectedTaskId = m_taskTable->item(row, 9)->text();
        } else {
            m_selectedTaskId.clear();
        }
    });
}

QString MainWindow::getValidFileName(const QString &url, const QString &saveDir)
{
    QUrl qurl(url);
    QString fileName = QFileInfo(qurl.path()).fileName();
    if (fileName.isEmpty()) fileName = "downloaded_file";

    QString baseName = QFileInfo(fileName).baseName();
    QString suffix = QFileInfo(fileName).suffix();
    if (!suffix.isEmpty()) suffix = "." + suffix;
    QString finalPath = QDir(saveDir).filePath(fileName);
    int counter = 1;
    while (QFile::exists(finalPath)) {
        finalPath = QDir(saveDir).filePath(baseName + QString(" (%1)").arg(counter) + suffix);
        counter++;
    }
    return finalPath;
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

    QString saveDir = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (saveDir.isEmpty()) saveDir = QDir::homePath() + "/Downloads";
    QDir().mkpath(saveDir);

    QString savePath = getValidFileName(urlStr, saveDir);
    QString fileName = QFileInfo(savePath).fileName();

    int threads = 8;  // 默认8线程

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

    QMessageBox::information(this, "提示", QString("已添加下载任务\n保存位置: %1").arg(savePath));
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
        bool delFile = QMessageBox::question(this, "删除", "是否同时删除已下载的文件？") == QMessageBox::Yes;
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
    if (m_selectedTaskId.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选中一个任务");
        return;
    }

    QString savePath;
    for (auto t : DownloadManager::instance()->getAllTasks()) {
        if (t.id == m_selectedTaskId) {
            savePath = t.savePath;
            break;
        }
    }

    if (savePath.isEmpty()) {
        QMessageBox::warning(this, "错误", "未找到任务文件路径");
        return;
    }

    QFileInfo fileInfo(savePath);
    QString dirPath = fileInfo.absoluteDir().absolutePath();
    if (!QDir(dirPath).exists()) {
        QMessageBox::warning(this, "错误", "目录不存在: " + dirPath);
        return;
    }

    bool opened = false;
    if (fileInfo.exists()) {
        opened = QDesktopServices::openUrl(QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
    }
    if (!opened) {
        opened = QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    }
    if (!opened) {
        QMessageBox::warning(this, "错误", "无法打开目录，请检查文件管理器是否正常");
    }
}

void MainWindow::onGlobalSpeedChanged(int idx)
{
    int speedKB = m_speedCombo->itemData(idx).toInt();
    int speedBytes = speedKB * 1024;
    DownloadManager::instance()->setGlobalSpeedLimit(speedBytes);
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

    m_taskTable->setItem(row, 0, new QTableWidgetItem(info.fileName));
    QProgressBar *bar = new QProgressBar();
    bar->setValue(0);
    bar->setFormat("%p%");
    m_taskTable->setCellWidget(row, 1, bar);
    m_taskTable->setItem(row, 2, new QTableWidgetItem("0 KB/s"));
    m_taskTable->setItem(row, 3, new QTableWidgetItem(formatSize(0)));
    m_taskTable->setItem(row, 4, new QTableWidgetItem(formatSize(info.totalSize)));
    m_taskTable->setItem(row, 5, new QTableWidgetItem("--"));
    m_taskTable->setItem(row, 6, new QTableWidgetItem(QString::number(info.threadCount)));
    m_taskTable->setItem(row, 7, new QTableWidgetItem(statusToString(info.status)));
    m_taskTable->setItem(row, 8, new QTableWidgetItem(info.savePath));
    QTableWidgetItem *idItem = new QTableWidgetItem(info.id);
    idItem->setFlags(Qt::NoItemFlags);
    m_taskTable->setItem(row, 9, idItem);

    m_taskRowMap[info.id] = row;
}

void MainWindow::onTaskProgress(const QString &taskId, int percent, qint64 speed)
{
    int row = findRowByTaskId(taskId);
    if (row < 0) return;

    QProgressBar *bar = qobject_cast<QProgressBar*>(m_taskTable->cellWidget(row, 1));
    if (bar) bar->setValue(percent);

    m_taskTable->item(row, 2)->setText(formatSpeed(speed));

    for (auto t : DownloadManager::instance()->getAllTasks()) {
        if (t.id == taskId) {
            m_taskTable->item(row, 3)->setText(formatSize(t.downloadedSize));
            if (speed > 0 && t.totalSize > t.downloadedSize) {
                qint64 remainSec = (t.totalSize - t.downloadedSize) / speed;
                if (remainSec < 60)
                    m_taskTable->item(row, 5)->setText(QString("%1 s").arg(remainSec));
                else if (remainSec < 3600)
                    m_taskTable->item(row, 5)->setText(QString("%1 m").arg(remainSec/60));
                else
                    m_taskTable->item(row, 5)->setText(QString("%1 h").arg(remainSec/3600));
            } else {
                m_taskTable->item(row, 5)->setText("--");
            }
            break;
        }
    }
}

void MainWindow::onTaskStatusChanged(const QString &taskId, TaskStatus status)
{
    int row = findRowByTaskId(taskId);
    if (row >= 0) m_taskTable->item(row, 7)->setText(statusToString(status));
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

    qint64 now = m_globalSpeedTimer.restart();
    if (now > 0) {
        qint64 deltaBytes = totalDownloaded - m_lastTotalDownloaded;
        qint64 globalSpeed = deltaBytes * 1000 / now;
        m_globalSpeedLabel->setText("总下载速度: " + formatSpeed(globalSpeed));
        m_lastTotalDownloaded = totalDownloaded;
    }
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

QString MainWindow::formatSpeed(qint64 bytesPerSec)
{
    if (bytesPerSec < 1024) return QString("%1 B/s").arg(bytesPerSec);
    if (bytesPerSec < 1024*1024) return QString("%1 KB/s").arg(bytesPerSec/1024);
    return QString("%1 MB/s").arg(bytesPerSec/1024/1024);
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