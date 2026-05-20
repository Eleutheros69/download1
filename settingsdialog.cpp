#include "settingsdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QSettings>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    loadSettings();
    connect(m_saveBtn, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_enableSpeedLimitCheck, &QCheckBox::toggled, this, &SettingsDialog::onSpeedLimitToggled);
}

SettingsDialog::~SettingsDialog() {}

void SettingsDialog::setupUI()
{
    setWindowTitle("设置");
    setMinimumWidth(450);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // 限速设置
    QGroupBox *speedGroup = new QGroupBox("全局限速");
    QVBoxLayout *speedLayout = new QVBoxLayout(speedGroup);
    m_enableSpeedLimitCheck = new QCheckBox("启用限速");
    speedLayout->addWidget(m_enableSpeedLimitCheck);
    QHBoxLayout *limitLayout = new QHBoxLayout();
    limitLayout->addWidget(new QLabel("限制速度:"));
    m_speedLimitCombo = new QComboBox();
    m_speedLimitCombo->addItem("100 KB/s", 100);
    m_speedLimitCombo->addItem("500 KB/s", 500);
    m_speedLimitCombo->addItem("1 MB/s", 1024);
    m_speedLimitCombo->addItem("2 MB/s", 2048);
    m_speedLimitCombo->addItem("5 MB/s", 5120);
    m_speedLimitCombo->addItem("10 MB/s", 10240);
    limitLayout->addWidget(m_speedLimitCombo);
    limitLayout->addStretch();
    speedLayout->addLayout(limitLayout);
    mainLayout->addWidget(speedGroup);

    // 并发任务数
    QGroupBox *concurrentGroup = new QGroupBox("任务调度");
    QHBoxLayout *concurrentLayout = new QHBoxLayout(concurrentGroup);
    concurrentLayout->addWidget(new QLabel("最大并行任务数:"));
    m_maxConcurrentSpin = new QSpinBox();
    m_maxConcurrentSpin->setRange(1, 10);
    concurrentLayout->addWidget(m_maxConcurrentSpin);
    concurrentLayout->addStretch();
    mainLayout->addWidget(concurrentGroup);

    // 默认分块线程数
    QGroupBox *threadGroup = new QGroupBox("分块下载");
    QHBoxLayout *threadLayout = new QHBoxLayout(threadGroup);
    threadLayout->addWidget(new QLabel("新任务默认线程数:"));
    m_defaultThreadsSpin = new QSpinBox();
    m_defaultThreadsSpin->setRange(1, 32);
    threadLayout->addWidget(m_defaultThreadsSpin);
    threadLayout->addStretch();
    mainLayout->addWidget(threadGroup);

    // 主题选择
    QGroupBox *themeGroup = new QGroupBox("界面主题");
    QHBoxLayout *themeLayout = new QHBoxLayout(themeGroup);
    themeLayout->addWidget(new QLabel("选择主题:"));
    m_themeCombo = new QComboBox();
    m_themeCombo->addItem("浅色主题", "light");
    m_themeCombo->addItem("暗黑主题", "dark");
    m_themeCombo->addItem("海洋蓝", "blue");
    themeLayout->addWidget(m_themeCombo);
    themeLayout->addStretch();
    mainLayout->addWidget(themeGroup);

    // 按钮
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_saveBtn = new QPushButton("保存");
    m_cancelBtn = new QPushButton("取消");
    btnLayout->addStretch();
    btnLayout->addWidget(m_saveBtn);
    btnLayout->addWidget(m_cancelBtn);
    mainLayout->addLayout(btnLayout);
}

void SettingsDialog::onSpeedLimitToggled(bool checked)
{
    m_speedLimitCombo->setEnabled(checked);
}

void SettingsDialog::loadSettings()
{
    QSettings settings("SijiStudio", "DownloadTool");
    bool enabled = settings.value("SpeedLimit/Enabled", false).toBool();
    int limitKB = settings.value("SpeedLimit/KB", 1024).toInt();
    int maxConcurrent = settings.value("MaxConcurrent", 3).toInt();
    int defaultThreads = settings.value("DefaultThreads", 8).toInt();
    QString theme = settings.value("Theme", "blue").toString();

    m_enableSpeedLimitCheck->setChecked(enabled);
    m_speedLimitCombo->setCurrentIndex(m_speedLimitCombo->findData(limitKB));
    m_speedLimitCombo->setEnabled(enabled);
    m_maxConcurrentSpin->setValue(maxConcurrent);
    m_defaultThreadsSpin->setValue(defaultThreads);

    int themeIndex = m_themeCombo->findData(theme);
    if (themeIndex >= 0) m_themeCombo->setCurrentIndex(themeIndex);
}

void SettingsDialog::saveSettings()
{
    QSettings settings("SijiStudio", "DownloadTool");
    settings.setValue("SpeedLimit/Enabled", m_enableSpeedLimitCheck->isChecked());
    settings.setValue("SpeedLimit/KB", m_speedLimitCombo->currentData().toInt());
    settings.setValue("MaxConcurrent", m_maxConcurrentSpin->value());
    settings.setValue("DefaultThreads", m_defaultThreadsSpin->value());
    settings.setValue("Theme", m_themeCombo->currentData().toString());

    accept();
    emit settingsChanged();
    emit themeChanged(getCurrentTheme());
}

int SettingsDialog::getSpeedLimitKB() const
{
    return m_enableSpeedLimitCheck->isChecked() ? m_speedLimitCombo->currentData().toInt() : 0;
}

int SettingsDialog::getMaxConcurrent() const
{
    return m_maxConcurrentSpin->value();
}

int SettingsDialog::getDefaultThreads() const
{
    return m_defaultThreadsSpin->value();
}

bool SettingsDialog::isSpeedLimitEnabled() const
{
    return m_enableSpeedLimitCheck->isChecked();
}

QString SettingsDialog::getCurrentTheme() const
{
    return m_themeCombo->currentData().toString();
}