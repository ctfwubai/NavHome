#include "FolderCardWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include <QFileInfo>
#include <QPixmap>
#include <QIcon>
#include <QSize>

namespace {
constexpr int kFolderCardWidth = 210;
constexpr int kFolderCardHeight = 78;
} // namespace

FolderCardWidget::FolderCardWidget(const Bookmark& folder, const QList<Bookmark>& items, QWidget* parent)
    : QFrame(parent), m_folder(folder), m_items(items) {
    setFixedSize(kFolderCardWidth, kFolderCardHeight);
    setObjectName("folderCard");
    setCursor(Qt::PointingHandCursor);
    setContextMenuPolicy(Qt::PreventContextMenu);
    buildUi();
}

void FolderCardWidget::buildUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 6, 10, 8);
    mainLayout->setSpacing(4);

    // 顶部：标题 + 悬停「解散」操作
    auto* top = new QHBoxLayout();
    top->setSpacing(4);
    auto* title = new QLabel(QString("文件夹 (%1)").arg(m_items.size()), this);
    title->setStyleSheet("font-size: 11px; color: #8a94a6;");
    top->addWidget(title);
    top->addStretch();

    m_actions = new QWidget(this);
    auto* actionLayout = new QHBoxLayout(m_actions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(4);
    auto* delBtn = new QToolButton(m_actions);
    delBtn->setText("✕");
    delBtn->setToolTip("解散文件夹");
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setStyleSheet("QToolButton { border: none; background: transparent; color: #e05a56; "
                          "font-size: 13px; } QToolButton:hover { color: #cf4a46; }");
    actionLayout->addWidget(delBtn);
    m_actions->hide();
    top->addWidget(m_actions);

    connect(delBtn, &QToolButton::clicked, this, [this] { emit deleteRequested(m_folder.id); });
    mainLayout->addLayout(top);

    // 图标行：展示成员图标（超出部分自然裁剪）
    auto* iconsRow = new QHBoxLayout();
    iconsRow->setSpacing(6);
    iconsRow->setContentsMargins(0, 0, 0, 0);

    for (const Bookmark& b : m_items) {
        auto* btn = new QToolButton(this);
        btn->setFixedSize(34, 34);
        btn->setIconSize(QSize(30, 30));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(b.url.isEmpty() ? b.name : b.name + "\n" + b.url);

        bool hasIcon = false;
        if (!b.icon.isEmpty() && QFileInfo::exists(b.icon)) {
            const QPixmap pm(b.icon);
            if (!pm.isNull()) {
                btn->setIcon(QIcon(pm.scaled(30, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
                btn->setStyleSheet("QToolButton { border: none; background: transparent; }");
                hasIcon = true;
            }
        }
        if (!hasIcon) {
            const QString initial = b.name.isEmpty() ? "?" : b.name.left(1);
            btn->setText(initial);
            btn->setStyleSheet("QToolButton { border: none; background: #e8f0fe; color: #2f6bdc; "
                               "border-radius: 8px; font-weight: bold; font-size: 14px; }");
        }

        connect(btn, &QToolButton::clicked, this, [this, b] {
            if (b.type == "app") {
                emit openAppRequested(b.url);
            } else {
                emit openRequested(b.url);
            }
        });
        iconsRow->addWidget(btn);
    }
    iconsRow->addStretch();
    mainLayout->addLayout(iconsRow, 1);
}

void FolderCardWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void FolderCardWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!(event->buttons() & Qt::LeftButton)) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    auto* mime = new QMimeData;
    mime->setData(kFolderMime, QByteArray::number(m_folder.id));
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(grab());
    drag->setHotSpot(event->pos());
    drag->exec(Qt::MoveAction);
}

void FolderCardWidget::enterEvent(QEnterEvent* event) {
    if (m_actions) {
        m_actions->show();
    }
    QFrame::enterEvent(event);
}

void FolderCardWidget::leaveEvent(QEvent* event) {
    if (m_actions) {
        m_actions->hide();
    }
    QFrame::leaveEvent(event);
}
