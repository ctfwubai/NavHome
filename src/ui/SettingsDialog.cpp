#include "SettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QComboBox>
#include <QLineEdit>
#include <QInputDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QFileInfo>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QMenu>
#include <QAction>
#include <QCursor>
#include <QSlider>

#include "app/AppConfig.h"
#include "app/PasswordManager.h"
#include "app/BackupManager.h"
#include "app/BookmarkImporter.h"
#include "app/BrowserLauncher.h"
#include "ui/BrowserSetupDialog.h"

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    m_pendingBrowser = AppConfig::instance().defaultBrowser();
    m_pendingBackground = AppConfig::instance().background();
    m_pendingTitleBarOpacity = AppConfig::instance().titleBarOpacity();
    buildUi();
}

void SettingsDialog::buildUi() {
    setWindowTitle("设置");
    setFixedWidth(460);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    // —— 默认浏览器 ——
    auto* browserTitle = new QLabel("默认浏览器", this);
    browserTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(browserTitle);

    auto* browserRow = new QHBoxLayout();
    m_browserIcon = new QLabel(this);
    m_browserIcon->setFixedSize(20, 20);
    m_browserIcon->setScaledContents(true);
    m_browserLabel = new QLabel(this);
    auto* chooseBtn = new QPushButton("更改…", this);
    browserRow->addWidget(m_browserIcon);
    browserRow->addWidget(m_browserLabel, 1);
    browserRow->addWidget(chooseBtn);
    layout->addLayout(browserRow);
    updateBrowserLabel();

    // —— 界面主题 ——
    auto* themeTitle = new QLabel("界面主题", this);
    themeTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(themeTitle);

    auto* themeRow = new QHBoxLayout();
    m_themeLightBtn = new QRadioButton("浅色", this);
    m_themeDarkBtn = new QRadioButton("深色", this);
    const QString curTheme = AppConfig::instance().theme();
    m_themeLightBtn->setChecked(curTheme != "dark");
    m_themeDarkBtn->setChecked(curTheme == "dark");
    themeRow->addWidget(m_themeLightBtn);
    themeRow->addWidget(m_themeDarkBtn);
    themeRow->addStretch();
    layout->addLayout(themeRow);

    // —— 背景设置 ——
    auto* bgTitle = new QLabel("背景设置", this);
    bgTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(bgTitle);

    auto* bgRow = new QHBoxLayout();
    m_bgCombo = new QComboBox(this);
    m_bgCombo->addItem("默认（跟随主题）", "");
    m_bgCombo->addItem("晨曦蓝", "preset:dawn");
    m_bgCombo->addItem("森林绿", "preset:forest");
    m_bgCombo->addItem("暮色紫", "preset:dusk");
    m_bgCombo->addItem("落日橙", "preset:sunset");
    m_bgCombo->addItem("自定义图片…", "__custom__");
    bgRow->addWidget(m_bgCombo, 1);
    auto* chooseImageBtn = new QPushButton("选择图片…", this);
    bgRow->addWidget(chooseImageBtn);
    layout->addLayout(bgRow);

    m_bgLabel = new QLabel("", this);
    m_bgLabel->setStyleSheet("color: #9aa3b2; font-size: 12px;");
    m_bgLabel->setWordWrap(true);
    layout->addWidget(m_bgLabel);

    // 同步当前背景到控件
    syncComboToBackground();
    updateBackgroundLabel();

    // —— 标题栏透明度 ——
    auto* tbTitle = new QLabel("标题栏透明度", this);
    tbTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(tbTitle);

    auto* tbRow = new QHBoxLayout();
    m_titleBarOpacitySlider = new QSlider(Qt::Horizontal, this);
    m_titleBarOpacitySlider->setRange(0, 100);
    m_titleBarOpacitySlider->setValue(m_pendingTitleBarOpacity);
    m_titleBarOpacityValue = new QLabel(QString::number(m_pendingTitleBarOpacity) + "%", this);
    m_titleBarOpacityValue->setFixedWidth(42);
    m_titleBarOpacityValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    tbRow->addWidget(m_titleBarOpacitySlider, 1);
    tbRow->addWidget(m_titleBarOpacityValue);
    layout->addLayout(tbRow);

    // —— 密码保护 ——
    auto* secTitle = new QLabel("密码保护", this);
    secTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(secTitle);

    auto* pwdRow = new QHBoxLayout();
    m_passwordCheck = new QCheckBox("启用密码保护（启动时需输入密码）", this);
    m_passwordCheck->setChecked(PasswordManager::isEnabled());
    auto* setPwdBtn = new QPushButton("设置密码", this);
    pwdRow->addWidget(m_passwordCheck, 1);
    pwdRow->addWidget(setPwdBtn);
    layout->addLayout(pwdRow);

    auto* hint = new QLabel("提示：忘记密码可通过程序目录下的 config.ini 手工修改找回。", this);
    hint->setStyleSheet("color: #9aa3b2; font-size: 12px;");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    // —— 数据备份 ——
    auto* backupTitle = new QLabel("数据备份", this);
    backupTitle->setStyleSheet("font-weight: 600;");
    layout->addWidget(backupTitle);

    auto* backupRow = new QHBoxLayout();
    auto* backupBtn = new QPushButton("立即备份…", this);
    auto* restoreBtn = new QPushButton("从备份恢复…", this);
    backupRow->addWidget(backupBtn);
    backupRow->addWidget(restoreBtn);
    backupRow->addStretch();
    layout->addLayout(backupRow);

    auto* importRow = new QHBoxLayout();
    auto* importBtn = new QPushButton("导入浏览器书签…", this);
    importBtn->setToolTip("直接读取本机 Chrome / Edge 收藏，或从 HTML 文件批量导入");
    importRow->addWidget(importBtn);
    importRow->addStretch();
    layout->addLayout(importRow);

    layout->addStretch();

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    btnBox->button(QDialogButtonBox::Ok)->setText("确定");
    btnBox->button(QDialogButtonBox::Cancel)->setText("取消");
    layout->addWidget(btnBox);

    connect(chooseBtn, &QPushButton::clicked, this, &SettingsDialog::onChooseBrowser);
    connect(m_passwordCheck, &QCheckBox::toggled, this, &SettingsDialog::onTogglePassword);
    connect(setPwdBtn, &QPushButton::clicked, this, &SettingsDialog::onSetPassword);
    connect(backupBtn, &QPushButton::clicked, this, &SettingsDialog::onBackup);
    connect(restoreBtn, &QPushButton::clicked, this, &SettingsDialog::onRestore);
    connect(importBtn, &QPushButton::clicked, this, &SettingsDialog::onImportBookmarks);
    connect(m_bgCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onBackgroundPresetChanged);
    connect(chooseImageBtn, &QPushButton::clicked, this, &SettingsDialog::onChooseBackgroundImage);
    connect(m_themeLightBtn, &QRadioButton::clicked, this, &SettingsDialog::emitPreview);
    connect(m_themeDarkBtn, &QRadioButton::clicked, this, &SettingsDialog::emitPreview);
    connect(m_titleBarOpacitySlider, &QSlider::valueChanged, this, [this](int v) {
        m_pendingTitleBarOpacity = v;
        m_titleBarOpacityValue->setText(QString::number(v) + "%");
        emitPreview();
    });
    connect(btnBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
}

void SettingsDialog::onChooseBrowser() {
    BrowserSetupDialog dlg(this);
    dlg.setCurrentPath(m_pendingBrowser);
    if (dlg.exec() == QDialog::Accepted) {
        m_pendingBrowser = dlg.selectedPath();
        updateBrowserLabel();
    }
}

void SettingsDialog::emitPreview() {
    emit previewChanged(m_themeDarkBtn->isChecked(), m_pendingBackground, m_pendingTitleBarOpacity);
}

void SettingsDialog::onTogglePassword(bool checked) {
    if (checked) {
        // 首次启用且尚未设置密码时，引导设置
        if (AppConfig::instance().password().isEmpty()) {
            if (!promptSetPassword()) {
                m_passwordCheck->blockSignals(true);
                m_passwordCheck->setChecked(false);
                m_passwordCheck->blockSignals(false);
            }
        }
    } else {
        PasswordManager::disable();
    }
}

void SettingsDialog::onSetPassword() {
    promptSetPassword();
}

bool SettingsDialog::promptSetPassword() {
    bool ok1 = false;
    const QString p1 = QInputDialog::getText(this, "设置密码", "请输入密码：",
                                             QLineEdit::Password, "", &ok1);
    if (!ok1) {
        return false;
    }
    bool ok2 = false;
    const QString p2 = QInputDialog::getText(this, "确认密码", "请再次输入密码：",
                                             QLineEdit::Password, "", &ok2);
    if (!ok2) {
        return false;
    }
    if (p1.isEmpty()) {
        QMessageBox::warning(this, "提示", "密码不能为空。");
        return false;
    }
    if (p1 != p2) {
        QMessageBox::warning(this, "提示", "两次输入的密码不一致。");
        return false;
    }
    // 默认使用哈希存储（更安全）；如确需明文可在 config.ini 中配置
    PasswordManager::enable(p1, true);
    return true;
}

void SettingsDialog::accept() {
    AppConfig::instance().setDefaultBrowser(m_pendingBrowser);
    AppConfig::instance().setBrowserConfigured(true);
    AppConfig::instance().setTheme(m_themeDarkBtn->isChecked() ? "dark" : "light");
    AppConfig::instance().setBackground(m_pendingBackground);
    AppConfig::instance().setTitleBarOpacity(m_pendingTitleBarOpacity);
    AppConfig::instance().sync();
    emit themeChanged();
    emit backgroundChanged();
    QDialog::accept();
}

QString SettingsDialog::browserDisplayName(const QString& path) const {
    if (path.isEmpty()) {
        return "系统默认浏览器";
    }
    return QFileInfo(path).completeBaseName();
}

void SettingsDialog::updateBrowserLabel() {
    m_browserLabel->setText(browserDisplayName(m_pendingBrowser));
    const QIcon icon = BrowserLauncher::iconForPath(m_pendingBrowser);
    if (!icon.isNull()) {
        m_browserIcon->setPixmap(icon.pixmap(20, 20));
    } else {
        m_browserIcon->clear();
    }
}

void SettingsDialog::onBackup() {
    const QString file = QFileDialog::getSaveFileName(this, "备份数据", "navhome-backup.json",
                                                      "JSON 文件 (*.json)");
    if (file.isEmpty()) {
        return;
    }
    QString err;
    if (BackupManager::exportData(file, &err)) {
        QMessageBox::information(this, "备份成功", "数据已备份到：\n" + file);
    } else {
        QMessageBox::warning(this, "备份失败", err);
    }
}

void SettingsDialog::onRestore() {
    const QString file = QFileDialog::getOpenFileName(this, "选择备份文件", "", "JSON 文件 (*.json)");
    if (file.isEmpty()) {
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle("恢复数据");
    box.setText("恢复将修改现有收藏数据，请选择恢复方式：");
    QPushButton* overwriteBtn = box.addButton("覆盖现有数据", QMessageBox::DestructiveRole);
    QPushButton* mergeBtn = box.addButton("合并到现有数据", QMessageBox::AcceptRole);
    QPushButton* cancelBtn = box.addButton("取消", QMessageBox::RejectRole);
    box.setDefaultButton(cancelBtn);
    box.exec();
    if (box.clickedButton() == cancelBtn) {
        return;
    }
    const BackupManager::ImportMode mode =
        (box.clickedButton() == overwriteBtn) ? BackupManager::Overwrite : BackupManager::Merge;

    QString err;
    if (BackupManager::importData(file, mode, &err)) {
        QMessageBox::information(this, "恢复成功", "数据已恢复，界面将刷新。");
        emit dataRestored();
    } else {
        QMessageBox::warning(this, "恢复失败", err);
    }
}

void SettingsDialog::onImportBookmarks() {
    // 弹出导入方式菜单：优先直接读取本机浏览器数据，其次从 HTML 文件导入
    QMenu menu(this);
    QAction* chromeAct = menu.addAction("从 Google Chrome 导入");
    QAction* edgeAct = menu.addAction("从 Microsoft Edge 导入");
    menu.addSeparator();
    QAction* htmlAct = menu.addAction("从 HTML 文件导入…");
    QAction* folderAct = menu.addAction("从文件夹中导入…");

    const QString chromePath = BookmarkImporter::chromeBookmarksPath();
    const QString edgePath = BookmarkImporter::edgeBookmarksPath();
    chromeAct->setEnabled(!chromePath.isEmpty());
    edgeAct->setEnabled(!edgePath.isEmpty());
    if (!chromeAct->isEnabled()) {
        chromeAct->setText("从 Google Chrome 导入（未检测到）");
    }
    if (!edgeAct->isEnabled()) {
        edgeAct->setText("从 Microsoft Edge 导入（未检测到）");
    }

    QPushButton* senderBtn = qobject_cast<QPushButton*>(sender());
    QAction* chosen = menu.exec(senderBtn ? senderBtn->mapToGlobal(QPoint(0, senderBtn->height()))
                                          : QCursor::pos());
    if (!chosen) {
        return;
    }

    BookmarkImporter::Result result;
    QString err;
    bool ok = false;
    if (chosen == chromeAct) {
        ok = BookmarkImporter::importChromeBookmarks(chromePath, &result, &err);
    } else if (chosen == edgeAct) {
        ok = BookmarkImporter::importChromeBookmarks(edgePath, &result, &err);
    } else if (chosen == folderAct) {
        const QString dir = QFileDialog::getExistingDirectory(this, "选择书签文件所在文件夹", "");
        if (dir.isEmpty()) {
            return;
        }
        ok = BookmarkImporter::importFolder(dir, &result, &err);
    } else {
        const QString file = QFileDialog::getOpenFileName(this, "选择浏览器导出的书签文件", "",
                                                          "HTML 文件 (*.html *.htm)");
        if (file.isEmpty()) {
            return;
        }
        ok = BookmarkImporter::import(file, &result, &err);
    }

    if (ok) {
        QMessageBox::information(
            this, "导入成功",
            QString("已导入 %1 个收藏，新建 %2 个分组。").arg(result.bookmarksImported).arg(result.groupsCreated));
        emit dataRestored();
    } else {
        QMessageBox::warning(this, "导入失败", err);
    }
}

void SettingsDialog::onBackgroundPresetChanged(int index) {
    const QString data = m_bgCombo->itemData(index).toString();
    if (data == "__custom__") {
        // 点击“自定义图片”项：若尚未选择图片则弹出文件选择
        if (!m_pendingBackground.startsWith("image:")) {
            onChooseBackgroundImage();
            return;
        }
        updateBackgroundLabel();
        emitPreview();
        return;
    }
    m_pendingBackground = data;
    updateBackgroundLabel();
    emitPreview();
}

void SettingsDialog::onChooseBackgroundImage() {
    const QString file = QFileDialog::getOpenFileName(
        this, "选择背景图片", "",
        "图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (file.isEmpty()) {
        // 取消：回退到当前背景对应项
        syncComboToBackground();
        return;
    }

    // 复制到数据目录下的 backgrounds/，保证系统重装后重新安装仍可稳定引用
    const QString dir = AppConfig::instance().dataDir() + "/backgrounds";
    QDir().mkpath(dir);
    const QString ext = QFileInfo(file).suffix().toLower();
    const QString dest =
        dir + "/bg_" + QString::number(QDateTime::currentMSecsSinceEpoch()) + "." + ext;
    if (QFile::copy(file, dest)) {
        m_pendingBackground = "image:" + dest;
    } else {
        // 复制失败则直接引用原路径
        m_pendingBackground = "image:" + file;
    }

    m_bgCombo->blockSignals(true);
    m_bgCombo->setCurrentIndex(m_bgCombo->findData("__custom__"));
    m_bgCombo->blockSignals(false);
    updateBackgroundLabel();
    emitPreview();
}

void SettingsDialog::syncComboToBackground() {
    m_bgCombo->blockSignals(true);
    if (m_pendingBackground.startsWith("image:")) {
        m_bgCombo->setCurrentIndex(m_bgCombo->findData("__custom__"));
    } else {
        const int idx = m_bgCombo->findData(m_pendingBackground);
        m_bgCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    m_bgCombo->blockSignals(false);
}

void SettingsDialog::updateBackgroundLabel() {
    if (m_pendingBackground.startsWith("image:")) {
        const QString path = m_pendingBackground.mid(6);
        m_bgLabel->setText("当前背景：自定义图片（" + QFileInfo(path).fileName() + "）");
    } else if (m_pendingBackground.isEmpty()) {
        m_bgLabel->setText("当前背景：默认");
    } else {
        m_bgLabel->setText("当前背景：" + m_bgCombo->currentText());
    }
}
