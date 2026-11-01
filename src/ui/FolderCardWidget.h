#pragma once

#include <QFrame>
#include <QList>
#include <QPoint>

#include "models/Bookmark.h"

// 文件夹拖拽 MIME 类型：携带文件夹 id（主窗口据此实现文件夹整体排序）
inline constexpr const char* kFolderMime = "application/x-navhome-folder";

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

private:
    void buildUi();

    Bookmark m_folder;
    QList<Bookmark> m_items;
    QPoint m_dragStartPos;
    QWidget* m_actions = nullptr;
};
