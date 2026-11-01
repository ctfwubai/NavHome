#include "AddAppDialog.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>

#include "app/BrowserLauncher.h"

AddAppDialog::AddAppDialog(const QList<Group>& groups, int currentGroupId, QWidget* parent)
    : QDialog(parent), m_groups(groups) {
    setWindowTitle("添加应用");
    buildUi();

    const int idx = m_groupCombo->findData(currentGroupId);
    if (idx >= 0) {
        m_groupCombo->setCurrentIndex(idx);
    }
}

void AddAppDialog::buildUi() {
    setFixedWidth(440);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(12);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("应用名称（留空自动取文件名）");

    // 程序路径行：输入框 + 浏览按钮
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText("程序路径，可带参数，如 D:\\Weixin\\Weixin.exe --scene=desktop");

    auto* browseBtn = new QPushButton("浏览…", this);
    browseBtn->setCursor(Qt::PointingHandCursor);

    auto* pathRow = new QHBoxLayout();
    pathRow->setSpacing(6);
    pathRow->addWidget(m_pathEdit, 1);
    pathRow->addWidget(browseBtn);

    m_groupCombo = new QComboBox(this);
    m_groupCombo->addItem("未分组", 0);
    for (const Group& g : m_groups) {
        m_groupCombo->addItem(g.name, g.id);
    }

    form->addRow("名称：", m_nameEdit);
    form->addRow("程序：", pathRow);
    form->addRow("分组：", m_groupCombo);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("完成");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    root->addWidget(buttons);

    connect(browseBtn, &QPushButton::clicked, this, &AddAppDialog::browse);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AddAppDialog::browse() {
    const QString path = QFileDialog::getOpenFileName(
        this, "选择应用程序", QString(),
        "应用程序 (*.exe *.lnk);;所有文件 (*)");
    if (path.isEmpty()) {
        return;
    }
    m_pathEdit->setText(QDir::toNativeSeparators(path));
    if (m_nameEdit->text().trimmed().isEmpty()) {
        m_nameEdit->setText(QFileInfo(path).completeBaseName());
    }
}

void AddAppDialog::setBookmark(const Bookmark& b) {
    setWindowTitle("编辑应用");
    m_original = b;
    m_nameEdit->setText(b.name);
    m_pathEdit->setText(b.url);
    const int idx = m_groupCombo->findData(b.groupId);
    if (idx >= 0) {
        m_groupCombo->setCurrentIndex(idx);
    }
}

Bookmark AddAppDialog::result() const {
    Bookmark b = m_original;
    b.type = "app";
    b.url = m_pathEdit->text().trimmed();
    b.name = m_nameEdit->text().trimmed();
    if (b.name.isEmpty()) {
        const QString exe = BrowserLauncher::exePathFromCommand(b.url);
        b.name = QFileInfo(exe).completeBaseName();
    }
    b.groupId = m_groupCombo->currentData().toInt();
    return b;
}

void AddAppDialog::accept() {
    const QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入程序路径。");
        m_pathEdit->setFocus();
        return;
    }
    // .lnk 允许存在即可；.exe 校验真实存在
    if (!path.endsWith(".lnk", Qt::CaseInsensitive) &&
        !QFileInfo::exists(BrowserLauncher::exePathFromCommand(path))) {
        QMessageBox::warning(this, "提示", "程序文件不存在，请检查路径。");
        m_pathEdit->setFocus();
        return;
    }
    QDialog::accept();
}
