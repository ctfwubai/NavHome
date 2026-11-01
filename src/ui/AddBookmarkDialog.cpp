#include "AddBookmarkDialog.h"

#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>

#include "app/BrowserLauncher.h"

AddBookmarkDialog::AddBookmarkDialog(const QList<Group>& groups, int currentGroupId, QWidget* parent)
    : QDialog(parent), m_groups(groups) {
    setWindowTitle("添加收藏");
    buildUi();

    // 默认选中当前分组
    const int idx = m_groupCombo->findData(currentGroupId);
    if (idx >= 0) {
        m_groupCombo->setCurrentIndex(idx);
    }
}

void AddBookmarkDialog::buildUi() {
    setFixedWidth(400);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(12);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("网站名称");

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText("例如 www.example.com");

    m_groupCombo = new QComboBox(this);
    m_groupCombo->addItem("未分组", 0);
    for (const Group& g : m_groups) {
        m_groupCombo->addItem(g.name, g.id);
    }

    form->addRow("名称：", m_nameEdit);
    form->addRow("网址：", m_urlEdit);
    form->addRow("分组：", m_groupCombo);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("完成");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    root->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AddBookmarkDialog::setBookmark(const Bookmark& b) {
    setWindowTitle("编辑收藏");
    m_original = b;
    m_nameEdit->setText(b.name);
    m_urlEdit->setText(b.url);
    const int idx = m_groupCombo->findData(b.groupId);
    if (idx >= 0) {
        m_groupCombo->setCurrentIndex(idx);
    }
}

Bookmark AddBookmarkDialog::result() const {
    Bookmark b = m_original;
    b.name = m_nameEdit->text().trimmed();
    b.url = BrowserLauncher::ensureScheme(m_urlEdit->text().trimmed());
    b.groupId = m_groupCombo->currentData().toInt();
    return b;
}

void AddBookmarkDialog::accept() {
    if (m_nameEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入名称。");
        m_nameEdit->setFocus();
        return;
    }
    if (m_urlEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入网址。");
        m_urlEdit->setFocus();
        return;
    }
    QDialog::accept();
}
