#include "AddFileDialog.h"

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
#include <QUrl>

#include "app/AppConfig.h"

namespace {

// 依据路径推导显示名：FTP 取主机名，目录取目录名，文件取去扩展名后的文件名
QString nameFromPath(const QString& p) {
    QString name;
    if (p.startsWith("ftp://", Qt::CaseInsensitive) || p.startsWith("ftps://", Qt::CaseInsensitive)) {
        name = QUrl(p).host();
    } else {
        const QFileInfo fi(p);
        name = fi.isDir() ? fi.fileName() : fi.completeBaseName();
    }
    return name.isEmpty() ? p : name;
}

} // namespace

AddFileDialog::AddFileDialog(const QList<Group>& groups, int currentGroupId, QWidget* parent)
    : QDialog(parent), m_groups(groups) {
    setWindowTitle("添加文件/文件夹");
    buildUi();

    const int idx = m_groupCombo->findData(currentGroupId);
    if (idx >= 0) {
        m_groupCombo->setCurrentIndex(idx);
    }
}

void AddFileDialog::buildUi() {
    setFixedWidth(460);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(16);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(12);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("名称（留空自动取文件名）");

    // 路径行：输入框 + 选择文件 / 选择文件夹
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText("文件 / 文件夹路径，也可填 \\\\server\\share 或 ftp://…");

    auto* fileBtn = new QPushButton("选择文件…", this);
    fileBtn->setCursor(Qt::PointingHandCursor);
    auto* folderBtn = new QPushButton("选择文件夹…", this);
    folderBtn->setCursor(Qt::PointingHandCursor);

    auto* pathRow = new QHBoxLayout();
    pathRow->setSpacing(6);
    pathRow->addWidget(m_pathEdit, 1);
    pathRow->addWidget(fileBtn);
    pathRow->addWidget(folderBtn);

    m_groupCombo = new QComboBox(this);
    m_groupCombo->addItem("未分组", 0);
    for (const Group& g : m_groups) {
        m_groupCombo->addItem(g.name, g.id);
    }

    form->addRow("名称：", m_nameEdit);
    form->addRow("路径：", pathRow);
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

    connect(fileBtn, &QPushButton::clicked, this, &AddFileDialog::browseFile);
    connect(folderBtn, &QPushButton::clicked, this, &AddFileDialog::browseFolder);
    connect(m_chooseIconBtn, &QPushButton::clicked, this, &AddFileDialog::chooseIcon);
    connect(m_clearIconBtn, &QPushButton::clicked, this, &AddFileDialog::clearIcon);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AddFileDialog::browseFile() {
    const QString path = QFileDialog::getOpenFileName(this, "选择文件", QString(), "所有文件 (*)");
    if (path.isEmpty()) {
        return;
    }
    m_pathEdit->setText(QDir::toNativeSeparators(path));
    autoFillName();
}

void AddFileDialog::browseFolder() {
    const QString path = QFileDialog::getExistingDirectory(this, "选择文件夹", QString());
    if (path.isEmpty()) {
        return;
    }
    m_pathEdit->setText(QDir::toNativeSeparators(path));
    autoFillName();
}

void AddFileDialog::autoFillName() {
    if (!m_nameEdit->text().trimmed().isEmpty()) {
        return;
    }
    const QString p = m_pathEdit->text().trimmed();
    if (p.isEmpty()) {
        return;
    }
    m_nameEdit->setText(nameFromPath(p));
}

void AddFileDialog::chooseIcon() {
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

void AddFileDialog::clearIcon() {
    m_customIcon.clear();
    m_iconTouched = true;
    m_iconPreview->setPixmap(QPixmap());
    m_iconPreview->setText("无");
}

QString AddFileDialog::copyIconToDataDir(const QString& src) {
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

void AddFileDialog::setBookmark(const Bookmark& b) {
    setWindowTitle("编辑文件/文件夹");
    m_original = b;
    m_nameEdit->setText(b.name);
    m_pathEdit->setText(b.url);
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

Bookmark AddFileDialog::result() const {
    Bookmark b = m_original;
    b.type = "file";
    b.url = m_pathEdit->text().trimmed();
    b.name = m_nameEdit->text().trimmed();
    if (b.name.isEmpty()) {
        b.name = nameFromPath(b.url);
    }
    b.groupId = m_groupCombo->currentData().toInt();
    // 自定义图标仅在用户改动时更新；未改动则保留原图标
    if (m_iconTouched) {
        b.icon = m_customIcon.isEmpty() ? QString() : copyIconToDataDir(m_customIcon);
    }
    return b;
}

void AddFileDialog::accept() {
    const QString path = m_pathEdit->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入文件或文件夹路径。");
        m_pathEdit->setFocus();
        return;
    }
    // FTP / UNC 共享允许目标暂不可达（需联网或认证），不强制本地存在
    const bool allowRemote = path.startsWith("ftp://", Qt::CaseInsensitive) ||
                             path.startsWith("ftps://", Qt::CaseInsensitive) ||
                             path.startsWith("\\\\");
    if (!allowRemote && !QFileInfo::exists(path)) {
        QMessageBox::warning(this, "提示", "路径不存在，请检查。");
        m_pathEdit->setFocus();
        return;
    }
    QDialog::accept();
}
