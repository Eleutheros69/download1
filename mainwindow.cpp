#include "mainwindow.h"
#include "settingsdialog.h"
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
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSettings>

// ==================== 全局样式表（增强） ====================
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
        border-radius: 8px;
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
    QMenuBar {
        background-color: #e9ecef;
        color: #212529;
    }
    QMenuBar::item {
        background-color: transparent;
        padding: 4px 8px;
    }
    QMenuBar::item:selected {
        background-color: #4CAF50;
        color: white;
    }
    QMenu {
        background-color: white;
        color: #212529;
        border: 1px solid #ced4da;
    }
    QMenu::item:selected {
        background-color: #4CAF50;
        color: white;
    }
)";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_lastTotalDownloaded(0), m_settingsDialog(nullptr)
{
    // 设置全局调色板
    QPalette pal = QApplication::palette();
    pal.setColor(QPalette::Window, QColor(240,242,245));
    pal.setColor(QPalette::WindowText, QColor(33,37,41));
    pal.setColor(QPalette::Base, QColor(255,255,255));
    pal.setColor(QPalette::Text, QColor(33,37,41));
    pal.setColor(QPalette::Button, QColor(76,175,80));
    pal.setColor(QPalette::ButtonText, Qt::white);
    QApplication::setPalette(pal);

    setupUI();
    createMenuBar();
    applySettings();

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

    // 工具栏（保留基本操作按钮，限速和并发移动到设置）
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
    connect(m_taskTable, &QTableWidget::itemSelectionChanged, [this](){
        int row = m_taskTable->currentRow();
        if (row >= 0) {
            m_selectedTaskId = m_taskTable->item(row, 9)->text();
        } else {
            m_selectedTaskId.clear();
        }
    });
}

void MainWindow::createMenuBar()
{
    QMenuBar *menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // 文件菜单
    QMenu *fileMenu = menuBar->addMenu("文件");
    QAction *newDownloadAction = fileMenu->addAction("新建下载");
    connect(newDownloadAction, &QAction::triggered, this, &MainWindow::onNewDownload);
    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction("退出");
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    // 设置菜单
    QMenu *settingsMenu = menuBar->addMenu("设置");
    QAction *prefAction = settingsMenu->addAction("首选项");
    connect(prefAction, &QAction::triggered, this, &MainWindow::showSettingsDialog);

    // 帮助菜单
    QMenu *helpMenu = menuBar->addMenu("帮助");
    QAction *aboutAction = helpMenu->addAction("关于");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);
}

void MainWindow::applySettings()
{
    QSettings settings("SijiStudio", "DownloadTool");
    // 限速
    bool speedLimitEnabled = settings.value("SpeedLimit/Enabled", false).toBool();
    int speedKB = settings.value("SpeedLimit/KB", 1024).toInt();
    if (speedLimitEnabled && speedKB > 0) {
        DownloadManager::instance()->setGlobalSpeedLimit(speedKB * 1024);
    } else {
        DownloadManager::instance()->setGlobalSpeedLimit(0);
    }
    // 最大并发
    int maxConcurrent = settings.value("MaxConcurrent", 3).toInt();
    DownloadManager::instance()->setMaxConcurrentTasks(maxConcurrent);
    // 默认线程数会用在新建下载中，直接读取即可
}

void MainWindow::showSettingsDialog()
{
    if (!m_settingsDialog) {
        m_settingsDialog = new SettingsDialog(this);
        connect(m_settingsDialog, &SettingsDialog::settingsChanged, this, &MainWindow::applySettings);
    }
    m_settingsDialog->exec();
}

void MainWindow::showAboutDialog()
{
    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle("关于");
    aboutBox.setTextFormat(Qt::RichText);
    aboutBox.setText(
        "<h3>多线程高速下载工具</h3>"
        "<p>版本 1.0</p>"
        "<p>开发者：四季工作室</p>"
        "<p>本软件基于 Qt 6 开发，遵循 GNU General Public License v3.0 开源协议。</p>"
        "<p>用户协议：<br>"
        "本软件仅供学习交流使用，请勿用于非法用途。下载内容版权归原作者所有。</p>"
        "<p>法律声明：<br>"
        "四季工作室不对因使用本软件造成的任何损失负责。</p>"
        "<p><a href=\"https://example.com/license\">查看完整许可证</a></p>"
        );
    aboutBox.setStandardButtons(QMessageBox::Ok);
    aboutBox.exec();
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

    // 从设置读取默认线程数
    QSettings settings("SijiStudio", "DownloadTool");
    int threads = settings.value("DefaultThreads", 8).toInt();

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
    if (m_selectedTaskId.isEmpty()) return;

    bool delFile = QMessageBox::question(this, "删除", "是否同时删除已下载的文件？") == QMessageBox::Yes;
    // 先停止任务（内部会停止所有分块线程）
    DownloadManager::instance()->pauseTask(m_selectedTaskId);
    // 调用取消删除
    DownloadManager::instance()->cancelTask(m_selectedTaskId, delFile);

    // 从表格移除
    int row = findRowByTaskId(m_selectedTaskId);
    if (row >= 0) {
        m_taskTable->removeRow(row);
        m_taskRowMap.remove(m_selectedTaskId);
    }
    m_selectedTaskId.clear();
    updateGlobalStats();
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

void MainWindow::onGlobalSpeedChanged(int idx)   // 不再需要，由设置对话框处理
{
    Q_UNUSED(idx);
}

void MainWindow::onMaxConcurrentChanged(int value)
{
    Q_UNUSED(value);
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