#pragma once

#include <QDialog>

#include "models/Bookmark.h"
#include "models/Group.h"

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;

// 添加 / 编辑文件（本地文件、文件夹、SMB/UNC 共享或 FTP 等网络路径）
class AddFileDialog : public QDialog {
    Q_OBJECT
public:
    AddFileDialog(const QList<Group>& groups, int currentGroupId, QWidget* parent = nullptr);

    // 编辑模式：预填信息
    void setBookmark(const Bookmark& b);

    // 获取填写结果（type 恒为 "file"）
    Bookmark result() const;

    // 本次是否设置了自定义图标（用于让调用方跳过自动提取）
    bool hasCustomIcon() const { return m_iconTouched && !m_customIcon.isEmpty(); }

private:
    void buildUi();
    void browseFile();
    void browseFolder();
    void accept() override;
    // 自定义图标：选择图标文件 / 清除自定义图标
    void chooseIcon();
    void clearIcon();
    // 把用户选择的图片复制到数据目录 icons/ 下，返回可持久化路径
    static QString copyIconToDataDir(const QString& src);
    // 名称留空时，依据路径自动填充名称
    void autoFillName();

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QComboBox* m_groupCombo = nullptr;
    QLabel* m_iconPreview = nullptr;
    QPushButton* m_chooseIconBtn = nullptr;
    QPushButton* m_clearIconBtn = nullptr;
    QString m_customIcon;      // 用户选择的图标源文件路径
    bool m_iconTouched = false; // 是否改动过图标（用于区分“未改”与“清除”）
    QList<Group> m_groups;
    Bookmark m_original;
};
