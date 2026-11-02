#pragma once

#include <QListWidget>

class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;

// 分组列表：支持分组间拖拽上下排序（内部移动），并承接从卡片区拖入的书签以移动分组
class GroupListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit GroupListWidget(QWidget* parent = nullptr);

signals:
    // 外部书签拖入某个分组（bookmarkId, 目标 groupId；groupId<0 表示“未分组”）
    void bookmarkDropped(int bookmarkId, int groupId);
    // 分组内部拖拽排序完成
    void groupOrderChanged();

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};
