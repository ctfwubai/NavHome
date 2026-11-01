#include "AddBookmarkDialog.h"

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
#include <QFile>
#include <QDateTime>
#include <QLabel>
#include <QPixmap>
#include <QCheckBox>

#include "app/BrowserLauncher.h"
#include "app/AppConfig.h"

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

    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText("登录账号（可选）");

    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText("登录密码（可选，本地加密保存）");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_showPwdCheck = new QCheckBox("显示", this);
    auto* pwdRow = new QHBoxLayout();
    pwdRow->setSpacing(6);
    pwdRow->addWidget(m_passwordEdit, 1);
    pwdRow->addWidget(m_showPwdCheck);

    form->addRow("名称：", m_nameEdit);
    form->addRow("网址：", m_urlEdit);
    form->addRow("账号：", m_usernameEdit);
    form->addRow("密码：", pwdRow);
    form->addRow("分组：", m_groupCombo);

    // 自定义图标行：预览 + 选择 + 清除
    auto* iconRow = new QHBoxLayout();
    iconRow->setSpacing(6);
    m_iconPreview = new QLabel(this);
    m_iconPreview->setFixedSize(40, 40);
    m_iconPreview->setAlignment(Qt::AlignCenter);
    m_iconPreview->setText("无");
    m_iconPreview->setStyleSheet("border:1px dashed #999; border-radius:4px;");
    m_chooseIconBtn = new QPushButton("选择…", this);
    m_chooseIconBtn->setCursor(Qt::PointingHandCursor);
    m_clearIconBtn = new QPushButton("清除", this);
    m_clearIconBtn->setCursor(Qt::PointingHandCursor);
    iconRow->addWidget(m_iconPreview);
    iconRow->addWidget(m_chooseIconBtn);
    iconRow->addWidget(m_clearIconBtn);
    iconRow->addStretch(1);
    form->addRow("自定义图标：", iconRow);
    root->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("完成");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    root->addWidget(buttons);

    connect(m_chooseIconBtn, &QPushButton::clicked, this, &AddBookmarkDialog::chooseIcon);
    connect(m_clearIconBtn, &QPushButton::clicked, this, &AddBookmarkDialog::clearIcon);
    connect(m_showPwdCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_passwordEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AddBookmarkDialog::chooseIcon() {
    const QString path = QFileDialog::getOpenFileName(
        this, "选择图标", QString(),
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.ico *.gif *.webp);;所有文件 (*)");
    if (path.isEmpty()) {
        return;
    }
    m_customIcon = path;
    m_iconTouched = true;
    QPixmap pm(path);
    if (!pm.isNull()) {
        m_iconPreview->setPixmap(pm.scaled(38, 38, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_iconPreview->setText("无");
    }
}

void AddBookmarkDialog::clearIcon() {
    m_customIcon.clear();
    m_iconTouched = true;
    m_iconPreview->setPixmap(QPixmap());
    m_iconPreview->setText("无");
}

QString AddBookmarkDialog::copyIconToDataDir(const QString& src) {
    if (src.isEmpty() || !QFileInfo::exists(src)) {
        return QString();
    }
    const QString dir = QDir(AppConfig::instance().dataDir()).filePath("icons");
    QDir().mkpath(dir);
    QString ext = QFileInfo(src).suffix().toLower();
    if (ext.isEmpty()) {
        ext = "png";
    }
    const QString dst = QDir(dir).filePath(
        QString("custom_%1.%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(ext));
    return QFile::copy(src, dst) ? dst : src;
}

void AddBookmarkDialog::setBookmark(const Bookmark& b) {
    setWindowTitle("编辑收藏");
    m_original = b;
    m_nameEdit->setText(b.name);
    m_urlEdit->setText(b.url);
    m_usernameEdit->setText(b.username);
    m_passwordEdit->setText(b.password);
    const int idx = m_groupCombo->findData(b.groupId);
    if (idx >= 0) {
        m_groupCombo->setCurrentIndex(idx);
    }
    // 预显示已有图标
    if (!b.icon.isEmpty() && QFileInfo::exists(b.icon)) {
        QPixmap pm(b.icon);
        if (!pm.isNull()) {
            m_iconPreview->setPixmap(pm.scaled(38, 38, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            return;
        }
    }
    m_iconPreview->setText("无");
}

Bookmark AddBookmarkDialog::result() const {
    Bookmark b = m_original;
    b.name = m_nameEdit->text().trimmed();
    b.url = BrowserLauncher::ensureScheme(m_urlEdit->text().trimmed());
    b.username = m_usernameEdit->text().trimmed();
    b.password = m_passwordEdit->text();
    b.groupId = m_groupCombo->currentData().toInt();
    // 自定义图标仅在用户改动时更新；未改动则保留原图标
    if (m_iconTouched) {
        b.icon = m_customIcon.isEmpty() ? QString() : copyIconToDataDir(m_customIcon);
    }
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
