#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include <QSpinBox>
#include <QTimer>
#include <QUuid>
#include <QMessageBox>
#include <QMap>
#include "downloadmanager.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onNewDownload();
    void onStart();
    void onPause();
    void onResume();
    void onCancel();
    void onOpenFolder();
    void onGlobalSpeedChanged(int value);
    void onMaxConcurrentChanged(int value);
    void onTaskAdded(const TaskInfo &info);
    void onTaskProgress(const QString &taskId, int percent, qint64 speed);
    void onTaskStatusChanged(const QString &taskId, TaskStatus status);
    void updateGlobalStats();

private:
    void setupUI();
    void addTaskToTable(const TaskInfo &info);
    int findRowByTaskId(const QString &taskId);
    static QString formatSize(qint64 bytes);
    static QString statusToString(TaskStatus status);

    // UI 组件
    QTableWidget *m_taskTable;
    QPushButton *m_newBtn, *m_startBtn, *m_pauseBtn, *m_resumeBtn, *m_cancelBtn, *m_openDirBtn;
    QComboBox *m_speedCombo;
    QSpinBox *m_concurrentSpin;
    QLabel *m_globalSpeedLabel, *m_globalProgressLabel;
    QMap<QString, int> m_taskRowMap;
    QString m_selectedTaskId;
};

#endif // MAINWINDOW_H