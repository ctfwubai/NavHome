#include "FlowLayout.h"

#include <QWidget>

FlowLayout::FlowLayout(QWidget* parent, int margin, int hSpacing, int vSpacing)
    : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing) {
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::FlowLayout(int margin, int hSpacing, int vSpacing)
    : m_hSpace(hSpacing), m_vSpace(vSpacing) {
    setContentsMargins(margin, margin, margin, margin);
}

FlowLayout::~FlowLayout() {
    QLayoutItem* item;
    while ((item = takeAt(0))) {
        delete item;
    }
}

void FlowLayout::addItem(QLayoutItem* item) {
    m_items.append(item);
}

int FlowLayout::count() const {
    return m_items.size();
}

QLayoutItem* FlowLayout::itemAt(int index) const {
    return m_items.value(index);
}

QLayoutItem* FlowLayout::takeAt(int index) {
    if (index >= 0 && index < m_items.size()) {
        return m_items.takeAt(index);
    }
    return nullptr;
}

int FlowLayout::horizontalSpacing() const {
    if (m_hSpace >= 0) {
        return m_hSpace;
    }
    return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
}

int FlowLayout::verticalSpacing() const {
    if (m_vSpace >= 0) {
        return m_vSpace;
    }
    return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
}

Qt::Orientations FlowLayout::expandingDirections() const {
    return {};
}

bool FlowLayout::hasHeightForWidth() const {
    return true;
}

int FlowLayout::heightForWidth(int width) const {
    return doLayout(QRect(0, 0, width, 0), true);
}

void FlowLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const {
    return minimumSize();
}

QSize FlowLayout::minimumSize() const {
    QSize size;
    for (const QLayoutItem* item : m_items) {
        size = size.expandedTo(item->minimumSize());
    }
    const QMargins margins = contentsMargins();
    size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
    return size;
}

int FlowLayout::doLayout(const QRect& rect, bool testOnly) const {
    int left, top, right, bottom;
    getContentsMargins(&left, &top, &right, &bottom);
    const QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
    int x = effectiveRect.x();
    int y = effectiveRect.y();
    int lineHeight = 0;

    for (QLayoutItem* item : m_items) {
        const QWidget* wid = item->widget();
        int spaceX = horizontalSpacing();
        if (spaceX == -1 && wid) {
            spaceX = wid->style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton,
                                                 Qt::Horizontal);
        }
        int spaceY = verticalSpacing();
        if (spaceY == -1 && wid) {
            spaceY = wid->style()->layoutSpacing(QSizePolicy::PushButton, QSizePolicy::PushButton,
                                                 Qt::Vertical);
        }

        int nextX = x + item->sizeHint().width() + spaceX;
        if (nextX - spaceX > effectiveRect.right() && lineHeight > 0) {
            x = effectiveRect.x();
            y = y + lineHeight + spaceY;
            nextX = x + item->sizeHint().width() + spaceX;
            lineHeight = 0;
        }

        if (!testOnly) {
            item->setGeometry(QRect(QPoint(x, y), item->sizeHint()));
        }

        x = nextX;
        lineHeight = qMax(lineHeight, item->sizeHint().height());
    }
    return y + lineHeight - rect.y() + bottom;
}

int FlowLayout::smartSpacing(QStyle::PixelMetric pm) const {
    QObject* parent = this->parent();
    if (!parent) {
        return -1;
    }
    if (parent->isWidgetType()) {
        QWidget* pw = static_cast<QWidget*>(parent);
        return pw->style()->pixelMetric(pm, nullptr, pw);
    }
    return static_cast<QLayout*>(parent)->spacing();
}

void FlowLayout::addWidget(QWidget* w) {
    QLayout::addWidget(w);
}

void FlowLayout::removeAllWidgets() {
    QLayoutItem* item;
    while ((item = takeAt(0))) {
        QWidget* w = item->widget();
        delete item;   // 先释放布局项
        if (w) {
            delete w;  // 再销毁控件
        }
    }
}

void FlowLayout::insertWidget(int index, QWidget* w) {
    // 源卡片通过 addWidget() 加入时已 addChildWidget，这里只调整布局顺序，避免重复登记
    m_items.insert(index, new QWidgetItem(w));
    invalidate();
}

QWidget* FlowLayout::takeWidgetAt(int index) {
    if (index < 0 || index >= m_items.size()) {
        return nullptr;
    }
    QLayoutItem* item = m_items.takeAt(index);
    QWidget* w = item ? item->widget() : nullptr;
    delete item;
    invalidate();
    return w;
}

int FlowLayout::indexOf(const QWidget* w) const {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i]->widget() == w) {
            return i;
        }
    }
    return -1;
}
