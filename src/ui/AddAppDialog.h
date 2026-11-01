#pragma once

#include <QDialog>

#include "models/Bookmark.h"
#include "models/Group.h"

class QLineEdit;
class QComboBox;

// 添加 / 编辑应用程序对话框（对应需求：添加应用，自动获取图标）
class AddAppDialog : public QDialog {
    Q_OBJECT
public:
    AddAppDialog(const QList<Group>& groups, int currentGroupId, QWidget* parent = nullptr);

    // 编辑模式：预填应用信息
    void setBookmark(const Bookmark& b);

    // 获取填写结果（type 恒为 "app"）
    Bookmark result() const;

private:
    void buildUi();
    void browse();
    void accept() override;

    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QComboBox* m_groupCombo = nullptr;
    QList<Group> m_groups;
    Bookmark m_original;
};
