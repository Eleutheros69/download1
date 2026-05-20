#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    int getSpeedLimitKB() const;
    int getMaxConcurrent() const;
    int getDefaultThreads() const;
    bool isSpeedLimitEnabled() const;
    QString getCurrentTheme() const;

signals:
    void settingsChanged();
    void themeChanged(const QString &theme);  // 主题改变信号

private slots:
    void onSpeedLimitToggled(bool checked);
    void saveSettings();
    void loadSettings();

private:
    void setupUI();

    QComboBox *m_speedLimitCombo;
    QCheckBox *m_enableSpeedLimitCheck;
    QSpinBox *m_maxConcurrentSpin;
    QSpinBox *m_defaultThreadsSpin;
    QComboBox *m_themeCombo;          // 主题选择下拉框
    QPushButton *m_saveBtn;
    QPushButton *m_cancelBtn;
};

#endif // SETTINGSDIALOG_H