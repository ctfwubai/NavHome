#include "GroupListWidget.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>

#include "ui/BookmarkCardWidget.h"  // kBookmarkMime

GroupListWidget::GroupListWidget(QWidget* parent) : QListWidget(parent) {
    // 分组内部拖拽排序；同时仍接受外部书签拖入（见 dragEnterEvent/dropEvent）
    setDragDropMode(QAbstractItemView::InternalMove);
    setDefaultDropAction(Qt::MoveAction);
}

void GroupListWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasFormat(kBookmarkMime)) {
        event->acceptProposedAction();
        return;
    }
    QListWidget::dragEnterEvent(event);
}

void GroupListWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasFormat(kBookmarkMime)) {
        event->acceptProposedAction();
        return;
    }
    QListWidget::dragMoveEvent(event);
}

void GroupListWidget::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasFormat(kBookmarkMime)) {
        const int id = event->mimeData()->data(kBookmarkMime).toInt();
        QListWidgetItem* item = itemAt(event->position().toPoint());
        int gid = 0;  // 拖到空白处或「全部」→ 移到“未分组”
        if (item) {
            gid = item->data(Qt::UserRole).toInt();
            if (gid < 0) {
                gid = 0;
            }
        }
        emit bookmarkDropped(id, gid);
        event->acceptProposedAction();
        return;
    }

    // 内部排序：先让 Qt 完成列表项移动，再通知主窗口持久化顺序
    QListWidget::dropEvent(event);
    emit groupOrderChanged();
}
