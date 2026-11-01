#pragma once

#include <QDialog>

#include "models/Bookmark.h"
#include "models/Group.h"

class QLineEdit;
class QComboBox;
class QLabel;
class QPushButton;

// 添加 / 编辑收藏对话框（对应需求 3.4.1 / 3.4.2）
class AddBookmarkDialog : public QDialog {
    Q_OBJECT
public:
    AddBookmarkDialog(const QList<Group>& groups, int currentGroupId, QWidget* parent = nullptr);

    // 编辑模式：预填收藏信息
    void setBookmark(const Bookmark& b);

    // 获取填写结果（编辑模式会保留原始 id）
    Bookmark result() const;

    // 本次是否设置了自定义图标（用于让调用方跳过自动提取）
    bool hasCustomIcon() const { return m_iconTouched && !m_customIcon.isEmpty(); }

private:
    void buildUi();
    void accept() override;
    // 自定义图标：选择图标文件 / 清除自定义图标
    void chooseIcon();
    void clearIcon();
    // 把用户选择的图片复制到数据目录 icons/ 下，返回可持久化路径
    static QString copyIconToDataDir(const QString& src);

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_urlEdit = nullptr;
    QComboBox* m_groupCombo = nullptr;
    QLabel* m_iconPreview = nullptr;
    QPushButton* m_chooseIconBtn = nullptr;
    QPushButton* m_clearIconBtn = nullptr;
    QString m_customIcon;      // 用户选择的图标源文件路径
    bool m_iconTouched = false; // 是否改动过图标（用于区分“未改”与“清除”）
    QList<Group> m_groups;
    Bookmark m_original;
};
