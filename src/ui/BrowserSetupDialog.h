#pragma once

#include <QDialog>
#include <QList>

#include "app/BrowserLauncher.h"

class QListWidget;

// 浏览器选择对话框：首次启动引导 + 设置中修改（对应需求 3.2）
class BrowserSetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit BrowserSetupDialog(QWidget* parent = nullptr);

    // 预选指定浏览器路径（空表示系统默认）
    void setCurrentPath(const QString& path);

    // 用户选择的浏览器路径（空表示使用系统默认）
    QString selectedPath() const;

private:
    void buildUi();

    QListWidget* m_list = nullptr;
    QList<BrowserInfo> m_browsers;
};
