#pragma once

#include <QFrame>
#include <QList>
#include <QPoint>

#include "models/Bookmark.h"

// 文件夹拖拽 MIME 类型：携带文件夹 id（主窗口据此实现文件夹整体排序）
inline constexpr const char* kFolderMime = "application/x-navhome-folder";
// 文件夹成员拖出 MIME 类型：携带 "folderId,bookmarkId"（用于把单个图标拖出文件夹）
inline constexpr const char* kFolderItemMime = "application/x-navhome-folder-item";

class QToolButton;

// 文件夹卡片：透明框，仅展示成员图标小网格；悬停图标显示描述，点击打开
class FolderCardWidget : public QFrame {
    Q_OBJECT
public:
    FolderCardWidget(const Bookmark& folder, const QList<Bookmark>& items, QWidget* parent = nullptr);

    int folderId() const { return m_folder.id; }
    const Bookmark& folder() const { return m_folder; }

signals:
    void openRequested(const QString& url);
    void openAppRequested(const QString& command);
    void deleteRequested(int id);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    // 拦截成员图标按钮的鼠标事件，实现「点击打开 / 拖出文件夹」
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void buildUi();

    Bookmark m_folder;
    QList<Bookmark> m_items;
    QPoint m_dragStartPos;   // 文件夹整体拖拽起点
    QPoint m_itemDragStart;  // 成员图标拖拽起点
    QWidget* m_actions = nullptr;
};
