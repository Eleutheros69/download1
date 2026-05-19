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

signals:
    void settingsChanged();  // 当设置改变时发出，供MainWindow更新

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
    QPushButton *m_saveBtn;
    QPushButton *m_cancelBtn;
};

#endif // SETTINGSDIALOG_H