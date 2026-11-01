#pragma once

#include <QFrame>
#include <QPoint>

#include "models/Bookmark.h"

// 拖拽 MIME 类型：携带收藏 id（主窗口以同一类型接收 drop，实现排序 / 跨分组拖拽）
inline constexpr const char* kBookmarkMime = "application/x-navhome-bookmark";

class QLabel;
class QToolButton;
class QDrag;
class QContextMenuEvent;

// 收藏卡片：图标 + 名称 + 网址，点击打开，悬停显示操作（对应需求 3.6）
class BookmarkCardWidget : public QFrame {
    Q_OBJECT
public:
    explicit BookmarkCardWidget(const Bookmark& bookmark, QWidget* parent = nullptr);

    int bookmarkId() const { return m_bookmark.id; }
    const Bookmark& bookmark() const { return m_bookmark; }

    bool isSelected() const { return m_selected; }
    void setSelected(bool selected);
    void setSelectionMode(bool enabled);

    // 由主窗口在“悬浮合并”触发时调用，结束进行中的拖拽
    void cancelDrag();

signals:
    void openRequested(const QString& url);
    void openAppRequested(const QString& command);
    void editRequested(int id);
    void deleteRequested(int id);
    void selectionToggled(int id, bool selected);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void buildUi();
    void updateIcon();
    void updateSelectedStyle();

    Bookmark m_bookmark;
    bool m_selected = false;
    bool m_selectionMode = false;
    QPoint m_dragStartPos;  // 按下位置，用于区分“点击打开”与“拖拽”
    QLabel* m_iconLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_urlLabel = nullptr;
    QWidget* m_actions = nullptr;
    QDrag* m_drag = nullptr;  // 进行中的拖拽对象，供主窗口取消
};
