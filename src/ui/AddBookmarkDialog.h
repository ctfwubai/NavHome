#pragma once

#include <QDialog>

#include "models/Bookmark.h"
#include "models/Group.h"

class QLineEdit;
class QComboBox;

// 添加 / 编辑收藏对话框（对应需求 3.4.1 / 3.4.2）
class AddBookmarkDialog : public QDialog {
    Q_OBJECT
public:
    AddBookmarkDialog(const QList<Group>& groups, int currentGroupId, QWidget* parent = nullptr);

    // 编辑模式：预填收藏信息
    void setBookmark(const Bookmark& b);

    // 获取填写结果（编辑模式会保留原始 id）
    Bookmark result() const;

private:
    void buildUi();
    void accept() override;

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_urlEdit = nullptr;
    QComboBox* m_groupCombo = nullptr;
    QList<Group> m_groups;
    Bookmark m_original;
};
