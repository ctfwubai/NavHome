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
class QLabel;

// 文件夹卡片：透明框，成员以正方形网格展示（图标 + 名称），点击打开、拖出文件夹；
// 悬停显示重命名/解散操作，双击标题重命名
class FolderCardWidget : public QFrame {
    Q_OBJECT
public:
    FolderCardWidget(const Bookmark& folder, const QList<Bookmark>& items, QWidget* parent = nullptr);

    int folderId() const { return m_folder.id; }
    const Bookmark& folder() const { return m_folder; }

    // 卡片尺寸随成员数量自适应（正方形 / 长方形），不写死固定大小
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void openRequested(const QString& url);
    void openAppRequested(const QString& command);
    void deleteRequested(int id);
    void renameRequested(int id);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
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
    QLabel* m_titleLabel = nullptr;
    QSize m_cardSize;  // 由 buildUi() 依据成员数量计算出的自适应尺寸
};
