#include "FolderCardWidget.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
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
#include <QPainter>
#include <QColor>
#include <QFontMetrics>
#include <cmath>

namespace {

// 成员方格尺寸（图标 + 名称，比旧版 34px 大，便于点击）
constexpr int kCellW = 76;
constexpr int kCellH = 82;
constexpr int kCellIcon = 42;
constexpr int kSpacing = 6;
constexpr int kMargin = 14;
constexpr int kTitleH = 26;

// 占位图标配色（与书签卡片一致）
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

// 生成带首字母的圆角占位图标
QPixmap placeholderPixmap(const QString& name, int size) {
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QString initial = name.isEmpty() ? "?" : name.left(1);
    uint hash = 0;
    for (const QChar& c : name) {
        hash = hash * 131 + c.unicode();
    }
    const PlaceholderColor& pc = kPlaceholderColors[hash % kPlaceholderColorCount];
    p.setBrush(QColor(pc.bg));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(0, 0, size, size, size / 5, size / 5);
    p.setPen(QColor(pc.fg));
    QFont f = p.font();
    f.setPixelSize(size / 2);
    f.setBold(true);
    p.setFont(f);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, initial);
    return pm;
}

} // namespace

FolderCardWidget::FolderCardWidget(const Bookmark& folder, const QList<Bookmark>& items, QWidget* parent)
    : QFrame(parent), m_folder(folder), m_items(items) {
    setObjectName("folderCard");
    setCursor(Qt::PointingHandCursor);
    setContextMenuPolicy(Qt::PreventContextMenu);
    buildUi();
}

QSize FolderCardWidget::sizeHint() const {
    return m_cardSize;
}

QSize FolderCardWidget::minimumSizeHint() const {
    return m_cardSize;
}

void FolderCardWidget::buildUi() {
    const int n = m_items.size();
    // 列数随成员数量自适应：成员少时呈长方形，增多后趋近正方形（ceil(sqrt(n)) 取最接近方形的排布）
    const int cols = n <= 1 ? 1 : static_cast<int>(std::ceil(std::sqrt(n)));
    const int rows = (n + cols - 1) / cols;

    const int cardW = cols * kCellW + (cols - 1) * kSpacing + 2 * kMargin;
    const int cardH = kTitleH + rows * kCellH + (rows - 1) * kSpacing + 2 * kMargin;
    m_cardSize = QSize(cardW, cardH);
    setMinimumSize(m_cardSize);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(kMargin, 6, kMargin, kMargin);
    mainLayout->setSpacing(6);

    // 顶部：标题 + 悬停「重命名 / 解散」操作
    auto* top = new QHBoxLayout();
    top->setSpacing(4);
    m_titleLabel = new QLabel(this);
    m_titleLabel->setText(m_folder.name.isEmpty() ? "文件夹" : m_folder.name);
    m_titleLabel->setToolTip("双击重命名");
    m_titleLabel->setStyleSheet("font-size: 12px; font-weight: 600; color: #8a94a6;");
    top->addWidget(m_titleLabel);

    auto* countLabel = new QLabel(QString("(%1)").arg(n), this);
    countLabel->setStyleSheet("font-size: 11px; color: #a0a9ba;");
    top->addWidget(countLabel);
    top->addStretch();

    m_actions = new QWidget(this);
    auto* actionLayout = new QHBoxLayout(m_actions);
    actionLayout->setContentsMargins(0, 0, 0, 0);
    actionLayout->setSpacing(4);

    auto* renameBtn = new QToolButton(m_actions);
    renameBtn->setText("✎");
    renameBtn->setToolTip("重命名");
    renameBtn->setCursor(Qt::PointingHandCursor);
    renameBtn->setStyleSheet("QToolButton { border: none; background: transparent; color: #4a6cf7; "
                             "font-size: 13px; } QToolButton:hover { color: #3553d6; }");
    actionLayout->addWidget(renameBtn);

    auto* delBtn = new QToolButton(m_actions);
    delBtn->setText("✕");
    delBtn->setToolTip("解散文件夹");
    delBtn->setCursor(Qt::PointingHandCursor);
    delBtn->setStyleSheet("QToolButton { border: none; background: transparent; color: #e05a56; "
                          "font-size: 13px; } QToolButton:hover { color: #cf4a46; }");
    actionLayout->addWidget(delBtn);

    m_actions->hide();
    top->addWidget(m_actions);
    mainLayout->addLayout(top);

    connect(renameBtn, &QToolButton::clicked, this, [this] { emit renameRequested(m_folder.id); });
    connect(delBtn, &QToolButton::clicked, this, [this] { emit deleteRequested(m_folder.id); });

    // 成员网格：图标在上、名称在下
    auto* grid = new QGridLayout();
    grid->setSpacing(kSpacing);
    grid->setContentsMargins(0, 0, 0, 0);

    for (int i = 0; i < n; ++i) {
        const Bookmark& b = m_items[i];
        auto* btn = new QToolButton(this);
        btn->setFixedSize(kCellW, kCellH);
        btn->setIconSize(QSize(kCellIcon, kCellIcon));
        btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setToolTip(b.name + (b.url.isEmpty() ? "" : "\n" + b.url));
        btn->setProperty("bookmarkId", b.id);
        btn->installEventFilter(this);

        // 图标：优先本地图标，否则首字母占位
        bool hasIcon = false;
        if (!b.icon.isEmpty() && QFileInfo::exists(b.icon)) {
            const QPixmap pm(b.icon);
            if (!pm.isNull()) {
                btn->setIcon(QIcon(pm.scaled(kCellIcon, kCellIcon, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation)));
                hasIcon = true;
            }
        }
        if (!hasIcon) {
            btn->setIcon(QIcon(placeholderPixmap(b.name, kCellIcon)));
        }

        // 名称：截断到方格宽度内
        const QFontMetrics fm(btn->font());
        btn->setText(fm.elidedText(b.name.isEmpty() ? "?" : b.name, Qt::ElideRight, kCellW - 6));
        btn->setStyleSheet("QToolButton { border: none; background: transparent; color: #3b4353; "
                           "font-size: 11px; }");

        connect(btn, &QToolButton::clicked, this, [this, b] {
            if (b.type == "app") {
                emit openAppRequested(b.url);
            } else {
                emit openRequested(b.url);
            }
        });
        grid->addWidget(btn, i / cols, i % cols);
    }
    mainLayout->addLayout(grid, 1);
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

void FolderCardWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    // 双击标题区域 → 重命名
    if (event->button() == Qt::LeftButton && m_titleLabel &&
        m_titleLabel->geometry().contains(event->pos())) {
        emit renameRequested(m_folder.id);
        event->accept();
        return;
    }
    QFrame::mouseDoubleClickEvent(event);
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

bool FolderCardWidget::eventFilter(QObject* obj, QEvent* event) {
    auto* btn = qobject_cast<QToolButton*>(obj);
    if (btn && btn->parent() == this && btn->property("bookmarkId").isValid()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_itemDragStart = me->pos();
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(event);
            if ((me->buttons() & Qt::LeftButton) &&
                (me->pos() - m_itemDragStart).manhattanLength() >= QApplication::startDragDistance()) {
                const int bookmarkId = btn->property("bookmarkId").toInt();
                auto* mime = new QMimeData;
                mime->setData(kFolderItemMime,
                              QString("%1,%2").arg(m_folder.id).arg(bookmarkId).toUtf8());
                auto* drag = new QDrag(btn);
                drag->setMimeData(mime);
                drag->setPixmap(btn->grab());
                drag->setHotSpot(me->pos());
                drag->exec(Qt::MoveAction);
                return true;  // 拖出文件夹：事件已处理，阻止按钮触发点击
            }
        }
    }
    return QFrame::eventFilter(obj, event);
}
