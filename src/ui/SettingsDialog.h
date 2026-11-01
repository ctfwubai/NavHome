#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QRadioButton;
class QSlider;

// 设置对话框：默认浏览器 + 密码保护（对应需求 3.2.2 / 3.7.2）
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

signals:
    // 数据恢复成功后发出，通知主窗口刷新
    void dataRestored();
    // 主题切换后发出，通知主窗口刷新样式
    void themeChanged();
    // 背景切换后发出，通知主窗口刷新样式
    void backgroundChanged();
    // 实时预览：主题/背景/标题栏透明度变化时发出（携带临时值，尚未写入配置）
    void previewChanged(bool dark, const QString& bg, int titleBarOpacity);

private slots:
    void onChooseBrowser();
    void onTogglePassword(bool checked);
    void onSetPassword();
    void onBackup();
    void onRestore();
    void onImportBookmarks();
    void onChooseBackgroundImage();
    void onBackgroundPresetChanged(int index);

private:
    void buildUi();
    bool promptSetPassword();
    void emitPreview();
    void accept() override;
    QString browserDisplayName(const QString& path) const;
    void updateBrowserLabel();
    void syncComboToBackground();
    void updateBackgroundLabel();

    QLabel* m_browserIcon = nullptr;
    QLabel* m_browserLabel = nullptr;
    QCheckBox* m_passwordCheck = nullptr;
    QRadioButton* m_themeLightBtn = nullptr;
    QRadioButton* m_themeDarkBtn = nullptr;
    QComboBox* m_bgCombo = nullptr;
    QLabel* m_bgLabel = nullptr;
    QSlider* m_titleBarOpacitySlider = nullptr;
    QLabel* m_titleBarOpacityValue = nullptr;
    QString m_pendingBrowser;
    QString m_pendingBackground;
    int m_pendingTitleBarOpacity = 100;
};
