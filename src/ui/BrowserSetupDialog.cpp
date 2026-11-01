#include "BrowserSetupDialog.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFrame>

BrowserSetupDialog::BrowserSetupDialog(QWidget* parent) : QDialog(parent) {
    buildUi();
}

void BrowserSetupDialog::buildUi() {
    setWindowTitle("选择默认浏览器");
    setFixedWidth(440);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto* title = new QLabel("选择默认浏览器", this);
    title->setStyleSheet("font-size: 16px; font-weight: 600;");
    layout->addWidget(title);

    auto* desc = new QLabel("点击收藏卡片时，将使用所选浏览器打开网址。", this);
    desc->setStyleSheet("color: #9aa3b2;");
    desc->setWordWrap(true);
    layout->addWidget(desc);

    m_list = new QListWidget(this);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setIconSize(QSize(20, 20));

    // 探测本机浏览器
    m_browsers = BrowserLauncher::detectBrowsers();
    const QString defaultName = BrowserLauncher::systemDefaultBrowserName();

    // 第一项：系统默认
    auto* sysItem = new QListWidgetItem("使用系统默认浏览器");
    sysItem->setData(Qt::UserRole, "");
    m_list->addItem(sysItem);

    int selectRow = 0;
    for (int i = 0; i < m_browsers.size(); ++i) {
        const BrowserInfo& info = m_browsers[i];
        QString label = info.name;
        if (!defaultName.isEmpty() && info.name == defaultName) {
            label += "（系统默认）";
        }
        auto* item = new QListWidgetItem(info.icon, label);
        item->setData(Qt::UserRole, info.path);
        m_list->addItem(item);
        if (!defaultName.isEmpty() && info.name == defaultName) {
            selectRow = i + 1;
        }
    }
    m_list->setCurrentRow(selectRow);
    layout->addWidget(m_list, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("确定");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void BrowserSetupDialog::setCurrentPath(const QString& path) {
    for (int i = 0; i < m_list->count(); ++i) {
        if (m_list->item(i)->data(Qt::UserRole).toString() == path) {
            m_list->setCurrentRow(i);
            return;
        }
    }
    m_list->setCurrentRow(0);
}

QString BrowserSetupDialog::selectedPath() const {
    QListWidgetItem* item = m_list->currentItem();
    if (!item) {
        return QString();
    }
    return item->data(Qt::UserRole).toString();
}
