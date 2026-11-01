#include "BookmarkCardWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QFileInfo>
#include <QPixmap>
#include <QDrag>
#include <QMimeData>
#include <QApplication>

#include "app/AppConfig.h"

namespace {
// 卡片固定尺寸（与需求“长方形圆角卡片”一致）
constexpr int kCardWidth = 210;
constexpr int kCardHeight = 78;

// 占位图标配色：淡色背景 + 深色前景，按名称散列取色（暖色多彩，参考 Notion 风格）
struct PlaceholderColor {
    const char* bg;
    const char* fg;
};
constexpr PlaceholderColor kPlaceholderColors[] = {
    {"#e8f0fe", "#2f6bdc"},  // 蓝
    {"#fdecec", "#c9453e"},  // 红
    {"#e6f6ec", "#2a9d5c"},  // 绿
    {"#f3ecfd", "#7a4fd0"},  // 紫
    {"#fef3e0", "#c77b1f"},  // 橙
    {"#e6f7f8", "#1f8a8f"},  // 青
};
constexpr int kPlaceholderColorCount =
    static_cast<int>(sizeof(kPlaceholderColors) / sizeof(kPlaceholderColors[0]));
} // namespace

BookmarkCardWidget::BookmarkCardWidget(const Bookmark& bookmark, QWidget* parent)
    : QFrame(parent), m_bookmark(bookmark) {
    setFixedSize(kCardWidth, kCardHeight);
    setObjectName("bookmarkCard");
    setCursor(Qt::PointingHandCursor);
    // 右键不响应（空白区域右键由主窗口处理“添加收藏”）
    setContextMenuPolicy(Qt::PreventContextMenu);
    buildUi();
    updateIcon();
}

void BookmarkCardWidget::buildUi() {
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 10, 8, 10);
    mainLayout->setSpacing(10);

    // 图标（48x48，圆角）
    m_iconLabel = new QLabel(this);
    m_iconLabel->setFixedSize(48, 48);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setObjectName("cardIcon");
    mainLayout->addWidget(m_iconLabel);

    // 名称 + 网址
    auto* textLayout = new QVBoxLayout();
    textLayout->setSpacing(3);
    m_nameLabel = new QLabel(m_bookmark.name, this);
    m_nameLabel->setObjectName("cardName");
    textLayout->addWidget(m_nameLabel);

    m_urlLabel = new QLabel(m_bookmark.url, this);
    m_urlLabel->setObjectName("cardUrl");
    textLayout->addWidget(m_urlLabel);

    mainLayout->addLayout(textLayout, 1);

    // 悬停操作按钮（编辑 / 删除）
    m_actions = new QWidget(this);
    m_actions->setFixedWidth(22);
    auto* actionLayout = new QVBoxLayout(m_actions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(4);

    auto* editBtn = new QToolButton(m_actions);
    editBtn->setText("✎");
    editBtn->setToolTip("编辑");
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setStyleSheet("QToolButton { border: none; background: transparent; color: #4a6cf7; "
                           "font-size: 14px; } QToolButton:hover { color: #3553d6; }");
    actionLayout->addWidget(editBtn);

    auto* delBtn = new QToolButton(m_actions);
    delBtn->setText("✕");
    delBtn->setToolTip("删除");
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setStyleSheet("QToolButton { border: none; background: transparent; color: #e05a56; "
                           "font-size: 14px; } QToolButton:hover { color: #cf4a46; }");
    actionLayout->addWidget(delBtn);

    mainLayout->addWidget(m_actions);
    m_actions->hide();  // 默认隐藏，悬停时显示

    connect(editBtn, &QToolButton::clicked, this, [this] { emit editRequested(m_bookmark.id); });
    connect(delBtn, &QToolButton::clicked, this, [this] { emit deleteRequested(m_bookmark.id); });
}

void BookmarkCardWidget::updateIcon() {
    // 有本地图标文件则直接显示，否则使用名称首字母作为占位图标
    if (!m_bookmark.icon.isEmpty() && QFileInfo::exists(m_bookmark.icon)) {
        QPixmap pm(m_bookmark.icon);
        if (!pm.isNull()) {
            m_iconLabel->setPixmap(pm.scaled(48, 48, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_iconLabel->setStyleSheet("background: transparent;");
            return;
        }
    }

    const QString initial = m_bookmark.name.isEmpty() ? "?" : m_bookmark.name.left(1);
    uint hash = 0;
    for (const QChar& c : m_bookmark.name) {
        hash = hash * 131 + c.unicode();
    }
    const PlaceholderColor& pc = kPlaceholderColors[hash % kPlaceholderColorCount];
    m_iconLabel->setText(initial);
    m_iconLabel->setStyleSheet(
        QString("background: %1; color: %2; border-radius: 12px; font-size: 20px; font-weight: bold;")
            .arg(pc.bg, pc.fg));
}

void BookmarkCardWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_selectionMode) {
            m_selected = !m_selected;
            updateSelectedStyle();
            emit selectionToggled(m_bookmark.id, m_selected);
        } else {
            // 记录按下位置，交给 mouseMove / mouseRelease 区分“点击打开”与“拖拽”
            m_dragStartPos = event->pos();
        }
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void BookmarkCardWidget::mouseMoveEvent(QMouseEvent* event) {
    if (m_selectionMode || !(event->buttons() & Qt::LeftButton)) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
        QFrame::mouseMoveEvent(event);
        return;
    }
    // 启动拖拽：MIME 携带收藏 id，主窗口据此实现排序 / 跨分组移动
    auto* mime = new QMimeData;
    mime->setData(kBookmarkMime, QByteArray::number(m_bookmark.id));
    auto* drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(grab());          // 拖拽时跟随显示卡片缩略图
    drag->setHotSpot(event->pos());
    drag->exec(Qt::MoveAction);
}

void BookmarkCardWidget::mouseReleaseEvent(QMouseEvent* event) {
    // 未发生拖拽（普通点击）时打开网址
    if (event->button() == Qt::LeftButton && !m_selectionMode) {
        emit openRequested(m_bookmark.url);
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

void BookmarkCardWidget::enterEvent(QEnterEvent* event) {
    if (m_actions) {
        m_actions->show();
    }
    QFrame::enterEvent(event);
}

void BookmarkCardWidget::leaveEvent(QEvent* event) {
    if (m_actions) {
        m_actions->hide();
    }
    QFrame::leaveEvent(event);
}

void BookmarkCardWidget::setSelected(bool selected) {
    if (m_selected == selected) {
        return;
    }
    m_selected = selected;
    updateSelectedStyle();
}

void BookmarkCardWidget::setSelectionMode(bool enabled) {
    m_selectionMode = enabled;
    setCursor(enabled ? Qt::ArrowCursor : Qt::PointingHandCursor);
    if (!enabled) {
        setSelected(false);
    }
}

void BookmarkCardWidget::updateSelectedStyle() {
    if (m_selected) {
        const bool dark = (AppConfig::instance().theme() == "dark");
        if (dark) {
            setStyleSheet("QFrame#bookmarkCard { background: #2d3b63; border: 2px solid #5a7dff; border-radius: 14px; }");
        } else {
            setStyleSheet("QFrame#bookmarkCard { background: #eaefff; border: 2px solid #4a6cf7; border-radius: 14px; }");
        }
    } else {
        setStyleSheet("");
    }
}
