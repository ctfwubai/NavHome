#include "MainWindow.h"

#include <QApplication>
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QLineEdit>
#include <QScrollArea>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>
#include <QFrame>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMenu>
#include <QAction>
#include <QMouseEvent>
#include <QWindow>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QIcon>
#include <QPixmap>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shobjidl.h>
#include <objbase.h>
#endif

#include "app/AppConfig.h"
#include "app/BrowserLauncher.h"
#include "app/IconFetcher.h"
#include "models/BookmarkRepository.h"
#include "ui/FlowLayout.h"
#include "ui/BookmarkCardWidget.h"
#include "ui/FolderCardWidget.h"
#include "ui/AddBookmarkDialog.h"
#include "ui/AddAppDialog.h"
#include "ui/BrowserSetupDialog.h"
#include "ui/SettingsDialog.h"

namespace {
// 拖拽数据是否包含可生成应用卡片的文件（.exe / .lnk）
bool hasAppFile(const QMimeData* mime) {
    if (!mime || !mime->hasUrls()) {
        return false;
    }
    for (const QUrl& url : mime->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString p = url.toLocalFile();
        if (p.endsWith(".exe", Qt::CaseInsensitive) || p.endsWith(".lnk", Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}
} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    m_iconFetcher = new IconFetcher(this);
    connect(m_iconFetcher, &IconFetcher::iconFetched, this, &MainWindow::onIconFetched);
    buildUi();
    refreshGroups();
    // 首次启动引导选择浏览器（延迟到事件循环，避免在 show 前弹窗）
    QTimer::singleShot(0, this, [this] { ensureBrowserConfigured(); });
}

void MainWindow::buildUi() {
    setWindowTitle("导航之家");
    setWindowFlags(Qt::FramelessWindowHint | Qt::Window | Qt::WindowSystemMenuHint |
                   Qt::WindowMinimizeButtonHint | Qt::WindowMaximizeButtonHint |
                   Qt::WindowCloseButtonHint);
    resize(1080, 720);

    auto* central = new QWidget(this);
    central->setObjectName("centralRoot");
    central->setAttribute(Qt::WA_StyledBackground, true);
    m_centralWidget = central;
    auto* outerLayout = new QVBoxLayout(central);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    // ===== 自绘标题栏（无边框窗口） =====
    m_titleBar = new QWidget(central);
    m_titleBar->setObjectName("titleBar");
    m_titleBar->setFixedHeight(40);
    m_titleBar->installEventFilter(this);
    auto* titleLayout = new QHBoxLayout(m_titleBar);
    titleLayout->setContentsMargins(14, 0, 4, 0);
    titleLayout->setSpacing(0);

    auto* titleLabel = new QLabel("导航之家", m_titleBar);
    titleLabel->setObjectName("titleBarTitle");
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();

    m_minBtn = new QPushButton("—", m_titleBar);
    m_maxBtn = new QPushButton("□", m_titleBar);
    m_closeBtn = new QPushButton("✕", m_titleBar);
    m_minBtn->setObjectName("titleBarBtn");
    m_maxBtn->setObjectName("titleBarBtn");
    m_closeBtn->setObjectName("titleBarCloseBtn");
    for (QPushButton* b : {m_minBtn, m_maxBtn, m_closeBtn}) {
        b->setFixedSize(46, 32);
        b->setCursor(Qt::PointingHandCursor);
        titleLayout->addWidget(b);
    }
    m_minBtn->setToolTip("最小化");
    m_maxBtn->setToolTip("最大化/还原");
    m_closeBtn->setToolTip("关闭");
    outerLayout->addWidget(m_titleBar);

    connect(m_minBtn, &QPushButton::clicked, this, [this] { showMinimized(); });
    connect(m_maxBtn, &QPushButton::clicked, this, &MainWindow::toggleMaximize);
    connect(m_closeBtn, &QPushButton::clicked, this, [this] {
        qInfo() << "标题栏：点击关闭";
        close();
    });

    auto* rootLayout = new QHBoxLayout();
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(12);
    outerLayout->addLayout(rootLayout, 1);

    // ===== 左侧：分组菜单 =====
    auto* leftPanel = new QWidget(central);
    m_leftPanel = leftPanel;
    leftPanel->setObjectName("leftPanel");
    leftPanel->setFixedWidth(200);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(12, 14, 12, 14);
    leftLayout->setSpacing(8);

    auto* groupTitle = new QLabel("分组", leftPanel);
    groupTitle->setStyleSheet("font-weight: 600; font-size: 14px;");
    leftLayout->addWidget(groupTitle);

    auto* groupBtnRow = new QHBoxLayout();
    groupBtnRow->setSpacing(4);
    auto* addGroupBtn = new QPushButton("＋", leftPanel);
    addGroupBtn->setObjectName("iconBtn");
    addGroupBtn->setToolTip("新增分组");
    addGroupBtn->setFixedSize(28, 28);
    auto* renameGroupBtn = new QPushButton("✎", leftPanel);
    renameGroupBtn->setObjectName("iconBtn");
    renameGroupBtn->setToolTip("重命名分组");
    renameGroupBtn->setFixedSize(28, 28);
    auto* delGroupBtn = new QPushButton("✕", leftPanel);
    delGroupBtn->setObjectName("iconBtn");
    delGroupBtn->setToolTip("删除分组");
    delGroupBtn->setFixedSize(28, 28);
    groupBtnRow->addWidget(addGroupBtn);
    groupBtnRow->addWidget(renameGroupBtn);
    groupBtnRow->addWidget(delGroupBtn);
    groupBtnRow->addStretch();
    leftLayout->addLayout(groupBtnRow);

    m_groupList = new QListWidget(leftPanel);
    m_groupList->setObjectName("groupList");
    m_groupList->setFrameShape(QFrame::NoFrame);
    m_groupList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_groupList->setAcceptDrops(true);           // 接收卡片拖拽（跨分组移动）
    m_groupList->installEventFilter(this);
    leftLayout->addWidget(m_groupList, 1);

    // 左下角：关于信息（版本号 + 作者，均为纯文字，灰色）
    auto* aboutLabel = new QLabel(
        QString("关于\n版本 V%1\n作者 无白\nhttps://github.com/ctfwubai")
            .arg(QApplication::applicationVersion()),
        leftPanel);
    aboutLabel->setStyleSheet("color: #9aa3b2; font-size: 11px;");
    aboutLabel->setWordWrap(true);
    leftLayout->addWidget(aboutLabel);

    connect(addGroupBtn, &QPushButton::clicked, this, &MainWindow::onAddGroup);
    connect(renameGroupBtn, &QPushButton::clicked, this, &MainWindow::onRenameGroup);
    connect(delGroupBtn, &QPushButton::clicked, this, &MainWindow::onDeleteGroup);
    connect(m_groupList, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem*, QListWidgetItem*) { onGroupSelected(); });

    // ===== 右侧：卡片展示区 =====
    auto* rightPanel = new QWidget(central);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(8);

    // 折叠/展开左侧栏按钮（始终可见，折叠后仍可再点展开）
    m_toggleSidebarBtn = new QPushButton("◀", rightPanel);
    m_toggleSidebarBtn->setFixedSize(34, 34);
    m_toggleSidebarBtn->setToolTip("折叠/展开左侧栏");
    toolbar->addWidget(m_toggleSidebarBtn);

    m_searchEdit = new QLineEdit(rightPanel);
    m_searchEdit->setPlaceholderText("搜索名称或网址…");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumHeight(34);

    auto* addBookmarkBtn = new QPushButton("＋ 添加收藏", rightPanel);
    addBookmarkBtn->setObjectName("primaryBtn");
    addBookmarkBtn->setMinimumHeight(34);
    m_selectModeBtn = new QPushButton("☑ 多选", rightPanel);
    m_selectModeBtn->setMinimumHeight(34);
    m_selectModeBtn->setCheckable(true);
    m_selectModeBtn->setToolTip("进入多选模式，批量删除或移动收藏");
    m_settingsBtn = new QPushButton("⚙ 设置", rightPanel);
    m_settingsBtn->setMinimumHeight(34);

    toolbar->addWidget(m_searchEdit, 1);
    toolbar->addWidget(addBookmarkBtn);
    toolbar->addWidget(m_selectModeBtn);
    toolbar->addWidget(m_settingsBtn);
    rightLayout->addLayout(toolbar);

    // 批量操作栏（多选模式下显示）
    m_batchBar = new QWidget(rightPanel);
    m_batchBar->setObjectName("batchBar");
    auto* batchLayout = new QHBoxLayout(m_batchBar);
    batchLayout->setContentsMargins(8, 4, 8, 4);
    batchLayout->setSpacing(8);
    m_batchInfoLabel = new QLabel("已选 0 项", m_batchBar);
    auto* batchSelectAllBtn = new QPushButton("全选", m_batchBar);
    m_batchMoveBtn = new QPushButton("移动到分组…", m_batchBar);
    m_batchDeleteBtn = new QPushButton("删除所选", m_batchBar);
    m_batchDeleteBtn->setObjectName("dangerBtn");
    batchLayout->addWidget(m_batchInfoLabel);
    batchLayout->addStretch();
    batchLayout->addWidget(batchSelectAllBtn);
    batchLayout->addWidget(m_batchMoveBtn);
    batchLayout->addWidget(m_batchDeleteBtn);
    m_batchBar->hide();
    rightLayout->addWidget(m_batchBar);

    connect(batchSelectAllBtn, &QPushButton::clicked, this, &MainWindow::onBatchSelectAll);
    connect(m_batchMoveBtn, &QPushButton::clicked, this, &MainWindow::onBatchMove);
    connect(m_batchDeleteBtn, &QPushButton::clicked, this, &MainWindow::onBatchDelete);

    m_emptyHint = new QLabel("暂无收藏，点击右上角「添加收藏」开始整理你的网页。", rightPanel);
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setStyleSheet("color: #9aa3b2; font-size: 14px;");
    m_emptyHint->setContextMenuPolicy(Qt::CustomContextMenu);  // 空态右键也能添加
    m_emptyHint->setAcceptDrops(true);       // 空态也支持拖入 .exe/.lnk 生成应用卡片
    m_emptyHint->installEventFilter(this);
    m_emptyHint->hide();
    rightLayout->addWidget(m_emptyHint, 1);

    m_scrollArea = new QScrollArea(rightPanel);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_gridContainer = new QWidget();
    m_gridContainer->setObjectName("gridContainer");
    m_gridContainer->setAcceptDrops(true);          // 接收卡片拖拽（排序）
    m_gridContainer->setContextMenuPolicy(Qt::CustomContextMenu);  // 空白处右键
    m_gridContainer->installEventFilter(this);
    m_gridLayout = new FlowLayout(m_gridContainer, 4, 10, 10);
    m_scrollArea->setWidget(m_gridContainer);
    rightLayout->addWidget(m_scrollArea, 1);

    connect(m_toggleSidebarBtn, &QPushButton::clicked, this, [this] {
        const bool visible = m_leftPanel->isVisible();
        m_leftPanel->setVisible(!visible);
        m_toggleSidebarBtn->setText(visible ? "▶" : "◀");
    });

    connect(m_searchEdit, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);
    connect(addBookmarkBtn, &QPushButton::clicked, this, &MainWindow::onAddBookmark);
    connect(m_selectModeBtn, &QPushButton::toggled, this, &MainWindow::onToggleSelectionMode);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onOpenSettings);
    connect(m_gridContainer, &QWidget::customContextMenuRequested, this, &MainWindow::onGridContextMenu);
    connect(m_emptyHint, &QWidget::customContextMenuRequested, this, &MainWindow::onGridContextMenu);

    rootLayout->addWidget(leftPanel);
    rootLayout->addWidget(rightPanel, 1);
    setCentralWidget(central);

    applyStyle();
}

void MainWindow::applyStyle() {
    applyStyle(AppConfig::instance().theme() == "dark",
               AppConfig::instance().background(),
               AppConfig::instance().titleBarOpacity());
}

void MainWindow::applyStyle(bool dark, const QString& bg, int titleBarOpacity) {
    const bool hasBg = !bg.isEmpty();

    // 仅主题或背景变化时才重设全局样式表（开销大）；
    // 标题栏透明度只影响标题栏背景，单独轻量刷新，保证滑块拖动丝滑
    const bool needFull = !m_styleInited || m_lastDark != dark || m_lastBg != bg;
    if (needFull) {
        // 有背景时左侧面板半透明，让背景透出；无背景时保持实色
        const QString leftPanelBg = dark
            ? (hasBg ? "rgba(38,43,51,0.88)" : "#262b33")
            : (hasBg ? "rgba(255,255,255,0.88)" : "#ffffff");

        // central 区域背景：预设渐变 / 自定义图片（拉伸铺满）/ 默认纯色
        QString centralStyle;
        if (bg.startsWith("image:")) {
            QString path = bg.mid(6);
            path.replace('\\', '/');
            const QString fallback = dark ? "#1e2228" : "#f5f7fa";
            centralStyle = QString(
                "QWidget#centralRoot { border-image: url(\"%1\") 0 0 0 0 stretch stretch; background: %2; }")
                .arg(path, fallback);
        } else if (bg.startsWith("preset:")) {
            const QString key = bg.mid(7);
            QString grad;
            if (key == "forest") {
                grad = "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #3fb27f, stop:1 #8fd9b0)";
            } else if (key == "dusk") {
                grad = "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #6c5ce7, stop:1 #b0a6f5)";
            } else if (key == "sunset") {
                grad = "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #f0932b, stop:1 #f7b58c)";
            } else {  // dawn
                grad = "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #5b8def, stop:1 #7ec8f5)";
            }
            centralStyle = "QWidget#centralRoot { background: " + grad + "; }";
        } else {
            centralStyle = QString("QWidget#centralRoot { background: %1; }")
                .arg(dark ? "#1e2228" : "#f5f7fa");
        }
        if (m_centralWidget) {
            m_centralWidget->setStyleSheet(centralStyle);
        }

        qApp->setStyleSheet(themeStylesheet(dark, leftPanelBg));
        applyToolbarStyle(dark, hasBg);

        m_lastDark = dark;
        m_lastBg = bg;
        m_styleInited = true;
    }

    // 标题栏背景：跟随主题深浅 + 透明度（无边框自绘标题栏）
    if (m_titleBar) {
        const int opacity = qBound(0, titleBarOpacity, 100);
        const int alpha = opacity * 255 / 100;
        const QString titleBarBg = dark
            ? QString("rgba(30,33,39,%1)").arg(alpha)
            : QString("rgba(255,255,255,%1)").arg(alpha);
        m_titleBar->setStyleSheet(QString(
            "QWidget#titleBar { background: %1; border-bottom: 1px solid rgba(128,128,128,0.18); }")
            .arg(titleBarBg));
    }
}

void MainWindow::applyToolbarStyle(bool dark, bool hasBg) {
    // 无背景时保持全局实色样式，不额外覆盖
    if (!hasBg) {
        m_searchEdit->setStyleSheet("");
        for (QPushButton* b : {m_toggleSidebarBtn, m_selectModeBtn, m_settingsBtn}) {
            if (b) {
                b->setStyleSheet("");
            }
        }
        return;
    }

    // 有背景（渐变或图片）时：顶部搜索框与普通按钮半透明，让背景透出
    QString searchStyle, btnStyle;
    if (dark) {
        searchStyle =
            "QLineEdit { background: rgba(38,42,49,0.55); border: 1px solid rgba(255,255,255,0.16); "
            "border-radius: 8px; padding: 7px 11px; color: #e6eaf0; }"
            "QLineEdit:focus { border-color: #5a7dff; background: rgba(38,42,49,0.75); }";
        btnStyle =
            "QPushButton { background: rgba(38,42,49,0.55); border: 1px solid rgba(255,255,255,0.16); "
            "border-radius: 8px; padding: 7px 15px; color: #d5dae4; }"
            "QPushButton:hover { background: rgba(50,55,65,0.7); }"
            "QPushButton:pressed { background: rgba(38,42,49,0.8); }";
    } else {
        searchStyle =
            "QLineEdit { background: rgba(255,255,255,0.55); border: 1px solid rgba(255,255,255,0.8); "
            "border-radius: 8px; padding: 7px 11px; color: #2b3040; }"
            "QLineEdit:focus { border-color: #4a6cf7; background: rgba(255,255,255,0.8); }";
        btnStyle =
            "QPushButton { background: rgba(255,255,255,0.55); border: 1px solid rgba(255,255,255,0.8); "
            "border-radius: 8px; padding: 7px 15px; color: #3b4353; }"
            "QPushButton:hover { background: rgba(255,255,255,0.75); }"
            "QPushButton:pressed { background: rgba(255,255,255,0.9); }";
    }
    m_searchEdit->setStyleSheet(searchStyle);
    for (QPushButton* b : {m_toggleSidebarBtn, m_selectModeBtn, m_settingsBtn}) {
        if (b) {
            b->setStyleSheet(btnStyle);
        }
    }
}

void MainWindow::applyGlobalStyle() {
    // 主窗口构造前调用，让登录对话框等早期界面也获得一致的样式
    const bool dark = (AppConfig::instance().theme() == "dark");
    const QString leftPanelBg = dark ? "#262b33" : "#ffffff";
    qApp->setStyleSheet(themeStylesheet(dark, leftPanelBg));
}

QString MainWindow::themeStylesheet(bool dark, const QString& leftPanelBg) {
    if (dark) {
        return QString(R"(
            QWidget { font-family: "Microsoft YaHei UI","Microsoft YaHei","Segoe UI",sans-serif; font-size: 13px; color: #d9dee7; }
            QMainWindow { background: #1a1c20; }
            QDialog, QMessageBox { background: #1f2227; }
            QPushButton { background: #262a31; border: 1px solid #343a45; border-radius: 8px; padding: 7px 15px; color: #d5dae4; }
            QPushButton:hover { background: #2d323b; border-color: #434b58; }
            QPushButton:pressed { background: #22262d; }
            QPushButton:disabled { color: #5a6270; background: #23262c; border-color: #2b2f36; }
            QPushButton#primaryBtn { background: #5a7dff; color: #ffffff; border: none; font-weight: 600; }
            QPushButton#primaryBtn:hover { background: #6b8bff; }
            QPushButton#primaryBtn:pressed { background: #4a6cf0; }
            QPushButton#primaryBtn:disabled { background: #3a4a7a; color: #8b93a8; }
            QPushButton#dangerBtn { background: #d95550; color: #ffffff; border: none; }
            QPushButton#dangerBtn:hover { background: #e26560; }
            QPushButton#dangerBtn:pressed { background: #c24743; }
            QLineEdit { background: #262a31; border: 1px solid #343a45; border-radius: 8px; padding: 7px 11px; color: #e6eaf0; selection-background-color: #5a7dff; selection-color: #ffffff; }
            QLineEdit:focus { border-color: #5a7dff; }
            QComboBox { background: #262a31; border: 1px solid #343a45; border-radius: 8px; padding: 6px 11px; color: #e6eaf0; }
            QComboBox:focus { border-color: #5a7dff; }
            QComboBox QAbstractItemView { background: #262a31; border: 1px solid #434b58; border-radius: 8px; selection-background-color: #2d3b63; selection-color: #dbe2ff; outline: 0; padding: 4px; }
            QListWidget { background: transparent; border: none; outline: 0; }
            QListWidget::item { padding: 9px 12px; border-radius: 8px; color: #c2c8d4; }
            QListWidget::item:hover { background: #262a31; }
            QListWidget::item:selected { background: #2d3b63; color: #dbe2ff; }
            QScrollArea { background: transparent; }
            QWidget#gridContainer { background: transparent; }
            QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
            QScrollBar::handle:vertical { background: #3a414c; border-radius: 5px; min-height: 28px; }
            QScrollBar::handle:vertical:hover { background: #4a525f; }
            QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
            QScrollBar::handle:horizontal { background: #3a414c; border-radius: 5px; min-width: 28px; }
            QScrollBar::handle:horizontal:hover { background: #4a525f; }
            QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; background: transparent; }
            QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
            QToolTip { background: #2d323b; color: #e6eaf0; border: 1px solid #434b58; border-radius: 6px; padding: 5px 9px; }
            QWidget#leftPanel { background: %1; border-radius: 14px; }
            QListWidget#groupList { background: transparent; border: none; outline: 0; font-size: 14px; }
            QListWidget#groupList::item { padding: 10px 12px; border-radius: 9px; color: #c2c8d4; }
            QListWidget#groupList::item:hover { background: #262a31; }
            QListWidget#groupList::item:selected { background: #2d3b63; color: #dbe2ff; font-weight: 600; }
            QPushButton#iconBtn { padding: 0; border-radius: 7px; font-size: 14px; }
            QFrame#bookmarkCard { background: #22262c; border: 1px solid #2d323a; border-radius: 14px; }
            QFrame#bookmarkCard:hover { border-color: #4a5d8f; background: #262b32; }
            QFrame#folderCard { background: rgba(255,255,255,0.06); border: 1px dashed #4a5d8f; border-radius: 14px; }
            QFrame#folderCard:hover { border-color: #6b8bff; background: rgba(255,255,255,0.10); }
            QWidget#batchBar { background: #3a2f26; border: 1px solid #5a4030; border-radius: 10px; }
            QLabel#cardName { font-weight: 600; font-size: 14px; color: #e6eaf0; }
            QLabel#cardUrl { font-size: 11px; color: #8a94a6; }
            QLabel#titleBarTitle { font-weight: 600; font-size: 13px; color: #d9dee7; }
            QPushButton#titleBarBtn { background: transparent; border: none; border-radius: 6px; padding: 0; color: #d5dae4; font-size: 15px; }
            QPushButton#titleBarBtn:hover { background: rgba(255,255,255,0.30); }
            QPushButton#titleBarBtn:pressed { background: rgba(255,255,255,0.42); }
            QPushButton#titleBarCloseBtn { background: transparent; border: none; border-radius: 6px; padding: 0; color: #d5dae4; font-size: 15px; }
            QPushButton#titleBarCloseBtn:hover { background: #e81123; color: #ffffff; }
            QPushButton#titleBarCloseBtn:pressed { background: #c50f1f; color: #ffffff; }
        )").arg(leftPanelBg);
    }
    return QString(R"(
        QWidget { font-family: "Microsoft YaHei UI","Microsoft YaHei","Segoe UI",sans-serif; font-size: 13px; color: #2b3040; }
        QMainWindow { background: #f2f4f8; }
        QDialog, QMessageBox { background: #f7f8fb; }
        QPushButton { background: #ffffff; border: 1px solid #e2e6ee; border-radius: 8px; padding: 7px 15px; color: #3b4353; }
        QPushButton:hover { background: #f3f6fb; border-color: #cfd6e2; }
        QPushButton:pressed { background: #e9eef6; }
        QPushButton:disabled { color: #a6afbf; background: #f5f6f9; border-color: #eceff3; }
        QPushButton#primaryBtn { background: #4a6cf7; color: #ffffff; border: none; font-weight: 600; }
        QPushButton#primaryBtn:hover { background: #3d5ce0; }
        QPushButton#primaryBtn:pressed { background: #3551c8; }
        QPushButton#primaryBtn:disabled { background: #a8b8f0; color: #ffffff; }
        QPushButton#dangerBtn { background: #e05a56; color: #ffffff; border: none; }
        QPushButton#dangerBtn:hover { background: #cf4a46; }
        QPushButton#dangerBtn:pressed { background: #b93e3a; }
        QLineEdit { background: #ffffff; border: 1px solid #e2e6ee; border-radius: 8px; padding: 7px 11px; color: #2b3040; selection-background-color: #4a6cf7; selection-color: #ffffff; }
        QLineEdit:focus { border-color: #4a6cf7; }
        QComboBox { background: #ffffff; border: 1px solid #e2e6ee; border-radius: 8px; padding: 6px 11px; color: #2b3040; }
        QComboBox:focus { border-color: #4a6cf7; }
        QComboBox QAbstractItemView { background: #ffffff; border: 1px solid #e2e6ee; border-radius: 8px; selection-background-color: #eaefff; selection-color: #3553d6; outline: 0; padding: 4px; }
        QListWidget { background: transparent; border: none; outline: 0; }
        QListWidget::item { padding: 9px 12px; border-radius: 8px; color: #4a5264; }
        QListWidget::item:hover { background: #eef1f6; }
        QListWidget::item:selected { background: #eaefff; color: #3553d6; }
        QScrollArea { background: transparent; }
        QWidget#gridContainer { background: transparent; }
        QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
        QScrollBar::handle:vertical { background: #c9d0dc; border-radius: 5px; min-height: 28px; }
        QScrollBar::handle:vertical:hover { background: #aeb8c8; }
        QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
        QScrollBar::handle:horizontal { background: #c9d0dc; border-radius: 5px; min-width: 28px; }
        QScrollBar::handle:horizontal:hover { background: #aeb8c8; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; background: transparent; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QToolTip { background: #2a2f3a; color: #f5f6f8; border: 1px solid #2a2f3a; border-radius: 6px; padding: 5px 9px; }
        QWidget#leftPanel { background: %1; border-radius: 14px; }
        QListWidget#groupList { background: transparent; border: none; outline: 0; font-size: 14px; }
        QListWidget#groupList::item { padding: 10px 12px; border-radius: 9px; color: #4a5264; }
        QListWidget#groupList::item:hover { background: #eef1f6; }
        QListWidget#groupList::item:selected { background: #eaefff; color: #3553d6; font-weight: 600; }
        QPushButton#iconBtn { padding: 0; border-radius: 7px; font-size: 14px; }
        QFrame#bookmarkCard { background: #ffffff; border: 1px solid #e7eaf0; border-radius: 14px; }
        QFrame#bookmarkCard:hover { border-color: #b9c8f2; background: #fdfdff; }
        QFrame#folderCard { background: rgba(255,255,255,0.35); border: 1px dashed #b9c8f2; border-radius: 14px; }
        QFrame#folderCard:hover { border-color: #4a6cf7; background: rgba(255,255,255,0.55); }
        QWidget#batchBar { background: #fff3ec; border: 1px solid #ffd8c0; border-radius: 10px; }
        QLabel#cardName { font-weight: 600; font-size: 14px; color: #2b3040; }
        QLabel#cardUrl { font-size: 11px; color: #98a1b3; }
        QLabel#titleBarTitle { font-weight: 600; font-size: 13px; color: #2b3040; }
        QPushButton#titleBarBtn { background: transparent; border: none; border-radius: 6px; padding: 0; color: #3b4353; font-size: 15px; }
        QPushButton#titleBarBtn:hover { background: #dbe3f0; }
        QPushButton#titleBarBtn:pressed { background: #c3cfe4; }
        QPushButton#titleBarCloseBtn { background: transparent; border: none; border-radius: 6px; padding: 0; color: #3b4353; font-size: 15px; }
        QPushButton#titleBarCloseBtn:hover { background: #e81123; color: #ffffff; }
        QPushButton#titleBarCloseBtn:pressed { background: #c50f1f; color: #ffffff; }
    )").arg(leftPanelBg);
}

void MainWindow::refreshGroups() {
    m_groups = BookmarkRepository::instance().groups();

    const int prevId = currentGroupId();
    m_groupList->blockSignals(true);
    m_groupList->clear();

    auto* allItem = new QListWidgetItem("全部");
    allItem->setData(Qt::UserRole, -1);
    m_groupList->addItem(allItem);

    for (const Group& g : m_groups) {
        auto* item = new QListWidgetItem(g.name);
        item->setData(Qt::UserRole, g.id);
        m_groupList->addItem(item);
    }

    // 恢复之前选中项
    int row = 0;
    for (int i = 0; i < m_groupList->count(); ++i) {
        if (m_groupList->item(i)->data(Qt::UserRole).toInt() == prevId) {
            row = i;
            break;
        }
    }
    m_groupList->setCurrentRow(row);
    m_groupList->blockSignals(false);

    refreshBookmarks();
}

void MainWindow::refreshBookmarks() {
    m_gridLayout->removeAllWidgets();

    const QString keyword = m_searchEdit->text().trimmed();
    QList<Bookmark> all;
    if (!keyword.isEmpty()) {
        all = BookmarkRepository::instance().search(keyword);
    } else {
        all = BookmarkRepository::instance().bookmarks(currentGroupId());
    }

    // 主网格只展示：文件夹 + 未被收纳进文件夹的收藏/应用；
    // 已收纳成员由文件夹卡片内部展示，不进 m_bookmarks，保证排序索引一致
    const QSet<int> members = BookmarkRepository::instance().folderMemberIds();
    m_bookmarks.clear();
    for (const Bookmark& b : all) {
        if (b.type == "folder" || !members.contains(b.id)) {
            m_bookmarks.append(b);
        }
    }

    for (const Bookmark& b : m_bookmarks) {
        if (b.type == "folder") {
            auto* card = new FolderCardWidget(b, BookmarkRepository::instance().folderItems(b.id));
            connect(card, &FolderCardWidget::openRequested, this, &MainWindow::onOpenUrl);
            connect(card, &FolderCardWidget::openAppRequested, this, &MainWindow::onOpenApp);
            connect(card, &FolderCardWidget::deleteRequested, this, &MainWindow::onDeleteFolder);
            m_gridLayout->addWidget(card);
        } else {
            auto* card = new BookmarkCardWidget(b);
            connect(card, &BookmarkCardWidget::openRequested, this, &MainWindow::onOpenUrl);
            connect(card, &BookmarkCardWidget::openAppRequested, this, &MainWindow::onOpenApp);
            connect(card, &BookmarkCardWidget::editRequested, this, &MainWindow::onEditBookmark);
            connect(card, &BookmarkCardWidget::deleteRequested, this, &MainWindow::onDeleteBookmark);
            connect(card, &BookmarkCardWidget::selectionToggled, this, &MainWindow::onSelectionToggled);
            card->setSelectionMode(m_selectionMode);
            card->setSelected(m_selectedIds.contains(b.id));
            m_gridLayout->addWidget(card);
        }
    }

    updateEmptyHint();
}

void MainWindow::updateEmptyHint() {
    const bool empty = m_bookmarks.isEmpty();
    m_emptyHint->setVisible(empty);
    m_scrollArea->setVisible(!empty);
}

int MainWindow::currentGroupId() const {
    QListWidgetItem* item = m_groupList->currentItem();
    if (!item) {
        return -1;
    }
    return item->data(Qt::UserRole).toInt();
}

const Group* MainWindow::findGroup(int id) const {
    for (const Group& g : m_groups) {
        if (g.id == id) {
            return &g;
        }
    }
    return nullptr;
}

Bookmark* MainWindow::findBookmark(int id) {
    for (Bookmark& b : m_bookmarks) {
        if (b.id == id) {
            return &b;
        }
    }
    return nullptr;
}

void MainWindow::ensureBrowserConfigured() {
    if (AppConfig::instance().browserConfigured()) {
        return;
    }
    BrowserSetupDialog dlg(this);
    dlg.exec();
    AppConfig::instance().setDefaultBrowser(dlg.selectedPath());
    AppConfig::instance().setBrowserConfigured(true);
    AppConfig::instance().sync();
}

// —— 槽函数 ——

void MainWindow::onAddGroup() {
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, "新增分组", "分组名称：", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    BookmarkRepository::instance().addGroup(name.trimmed());
    refreshGroups();
}

void MainWindow::onRenameGroup() {
    const int gid = currentGroupId();
    if (gid < 0) {
        QMessageBox::information(this, "提示", "「全部」不能重命名。");
        return;
    }
    const Group* g = findGroup(gid);
    if (!g) {
        return;
    }
    bool ok = false;
    const QString name =
        QInputDialog::getText(this, "重命名分组", "分组名称：", QLineEdit::Normal, g->name, &ok);
    if (!ok || name.trimmed().isEmpty()) {
        return;
    }
    BookmarkRepository::instance().renameGroup(gid, name.trimmed());
    refreshGroups();
}

void MainWindow::onDeleteGroup() {
    const int gid = currentGroupId();
    if (gid < 0) {
        QMessageBox::information(this, "提示", "「全部」不能删除。");
        return;
    }
    const Group* g = findGroup(gid);
    if (!g) {
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle("删除分组");
    box.setText(QString("确定删除分组「%1」吗？").arg(g->name));
    box.setInformativeText("组内收藏将如何处理？");
    QPushButton* moveBtn = box.addButton("移至未分组", QMessageBox::AcceptRole);
    QPushButton* delBtn = box.addButton("一并删除", QMessageBox::DestructiveRole);
    QPushButton* cancelBtn = box.addButton("取消", QMessageBox::RejectRole);
    box.setDefaultButton(cancelBtn);
    box.exec();

    if (box.clickedButton() == cancelBtn) {
        return;
    }
    const bool moveToUngrouped = (box.clickedButton() == moveBtn);
    BookmarkRepository::instance().deleteGroup(gid, moveToUngrouped);
    refreshGroups();
}

void MainWindow::onAddBookmark() {
    AddBookmarkDialog dlg(m_groups, currentGroupId(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    Bookmark b = dlg.result();
    const int id = BookmarkRepository::instance().addBookmark(b);
    if (id > 0) {
        m_iconFetcher->fetchForBookmark(id, b.url);
    }
    refreshBookmarks();
}

void MainWindow::onAddApp() {
    AddAppDialog dlg(m_groups, currentGroupId(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    Bookmark b = dlg.result();
    const int id = BookmarkRepository::instance().addBookmark(b);
    if (id > 0) {
        saveAppIcon(id, BrowserLauncher::exePathFromCommand(b.url));
    }
    refreshBookmarks();
}

void MainWindow::onEditBookmark(int id) {
    Bookmark* b = findBookmark(id);
    if (!b) {
        return;
    }
    const QString oldUrl = b->url;

    if (b->type == "app") {
        AddAppDialog dlg(m_groups, b->groupId, this);
        dlg.setBookmark(*b);
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        Bookmark updated = dlg.result();
        updated.id = id;
        BookmarkRepository::instance().updateBookmark(updated);
        if (updated.url != oldUrl) {
            saveAppIcon(id, BrowserLauncher::exePathFromCommand(updated.url));
        }
    } else {
        AddBookmarkDialog dlg(m_groups, b->groupId, this);
        dlg.setBookmark(*b);
        if (dlg.exec() != QDialog::Accepted) {
            return;
        }
        Bookmark updated = dlg.result();
        updated.id = id;
        BookmarkRepository::instance().updateBookmark(updated);
        if (updated.url != oldUrl) {
            m_iconFetcher->fetchForBookmark(id, updated.url);
        }
    }
    refreshBookmarks();
}

void MainWindow::onDeleteBookmark(int id) {
    const auto ret = QMessageBox::question(this, "删除收藏", "确定删除该收藏吗？",
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }
    BookmarkRepository::instance().deleteBookmark(id);
    refreshBookmarks();
}

void MainWindow::onDeleteFolder(int folderId) {
    const auto ret = QMessageBox::question(this, "解散文件夹",
                                           "确定解散该文件夹吗？其中的收藏将回到当前分组。",
                                           QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }
    BookmarkRepository::instance().deleteFolder(folderId);
    refreshBookmarks();
}

void MainWindow::createFolder(int aId, int bId) {
    // 仅单分组视图支持创建文件夹（“全部”视图跨分组，语义不清）
    if (currentGroupId() < 0) {
        return;
    }
    auto& repo = BookmarkRepository::instance();
    const int fid = repo.addFolder(currentGroupId());
    if (fid < 0) {
        return;
    }
    repo.addItemToFolder(fid, aId);
    repo.addItemToFolder(fid, bId);
    refreshBookmarks();
}

void MainWindow::addToFolder(int folderId, int bookmarkId) {
    if (currentGroupId() < 0) {
        return;
    }
    BookmarkRepository::instance().addItemToFolder(folderId, bookmarkId);
    refreshBookmarks();
}

void MainWindow::onGroupSelected() {
    refreshBookmarks();
}

void MainWindow::onFilterChanged(const QString&) {
    refreshBookmarks();
}

void MainWindow::onOpenUrl(const QString& url) {
    const QString browser = AppConfig::instance().defaultBrowser();
    if (!BrowserLauncher::openUrl(url, browser)) {
        QMessageBox::warning(this, "提示", "无法打开网址，请检查浏览器设置。");
    }
}

void MainWindow::onOpenApp(const QString& command) {
    if (!BrowserLauncher::openApp(command)) {
        QMessageBox::warning(this, "提示", "无法启动应用，请检查路径是否正确。");
    }
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_groupList) {
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto* e = static_cast<QDragMoveEvent*>(event);
            if (e->mimeData()->hasFormat(kBookmarkMime)) {
                e->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto* e = static_cast<QDropEvent*>(event);
            const int id = e->mimeData()->data(kBookmarkMime).toInt();
            QListWidgetItem* item = m_groupList->itemAt(e->position().toPoint());
            if (item) {
                int gid = item->data(Qt::UserRole).toInt();
                if (gid < 0) {
                    gid = 0;  // 拖到「全部」→ 移到“未分组”
                }
                if (BookmarkRepository::instance().moveBookmarks({id}, gid)) {
                    refreshGroups();
                }
                e->acceptProposedAction();
                return true;
            }
        }
    } else if (obj == m_gridContainer) {
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto* e = static_cast<QDragMoveEvent*>(event);
            if (e->mimeData()->hasFormat(kBookmarkMime) ||
                e->mimeData()->hasFormat(kFolderMime) ||
                e->mimeData()->hasFormat(kFolderItemMime) ||
                hasAppFile(e->mimeData())) {
                e->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto* e = static_cast<QDropEvent*>(event);
            if (e->mimeData()->hasFormat(kBookmarkMime)) {
                const int id = e->mimeData()->data(kBookmarkMime).toInt();
                const QPoint pos = e->position().toPoint();
                // 落在某卡片/文件夹上 → 创建或加入文件夹（拖到卡片上方松手）
                QWidget* target = nullptr;
                for (int i = 0; i < m_gridLayout->count(); ++i) {
                    QWidget* w = m_gridLayout->itemAt(i)->widget();
                    if (w && w->geometry().contains(pos)) {
                        target = w;
                        break;
                    }
                }
                if (target) {
                    if (auto* folder = qobject_cast<FolderCardWidget*>(target)) {
                        addToFolder(folder->folderId(), id);
                    } else if (auto* card = qobject_cast<BookmarkCardWidget*>(target)) {
                        if (card->bookmarkId() != id) {
                            createFolder(id, card->bookmarkId());
                        }
                    }
                    e->acceptProposedAction();
                    return true;
                }
                // 否则按排序处理：落在卡片之间（拖到前面 = 排序）
                int targetIndex = m_bookmarks.size();
                for (int i = 0; i < m_gridLayout->count(); ++i) {
                    QWidget* w = m_gridLayout->itemAt(i)->widget();
                    if (!w) {
                        continue;
                    }
                    const QRect r = w->geometry();
                    const bool before = (pos.y() < r.top())
                        || (pos.y() < r.bottom() && pos.x() < r.center().x());
                    if (before) {
                        targetIndex = i;
                        break;
                    }
                }
                reorderBookmark(id, targetIndex);
                e->acceptProposedAction();
                return true;
            }
            if (e->mimeData()->hasFormat(kFolderMime)) {
                const int id = e->mimeData()->data(kFolderMime).toInt();
                const QPoint pos = e->position().toPoint();
                int targetIndex = m_bookmarks.size();
                for (int i = 0; i < m_gridLayout->count(); ++i) {
                    QWidget* w = m_gridLayout->itemAt(i)->widget();
                    if (!w) {
                        continue;
                    }
                    const QRect r = w->geometry();
                    const bool before = (pos.y() < r.top())
                        || (pos.y() < r.bottom() && pos.x() < r.center().x());
                    if (before) {
                        targetIndex = i;
                        break;
                    }
                }
                reorderBookmark(id, targetIndex);
                e->acceptProposedAction();
                return true;
            }
            if (e->mimeData()->hasFormat(kFolderItemMime)) {
                const QString data = QString::fromUtf8(e->mimeData()->data(kFolderItemMime));
                const QStringList parts = data.split(',');
                if (parts.size() == 2) {
                    const int folderId = parts[0].toInt();
                    const int bookmarkId = parts[1].toInt();
                    auto& repo = BookmarkRepository::instance();
                    repo.removeItemFromFolder(folderId, bookmarkId);
                    // 文件夹空了则自动解散，成员回到分组
                    if (repo.folderItems(folderId).isEmpty()) {
                        repo.deleteFolder(folderId);
                    }
                    refreshBookmarks();
                }
                e->acceptProposedAction();
                return true;
            }
            if (hasAppFile(e->mimeData())) {
                handleAppFileDrop(e->mimeData());
                e->acceptProposedAction();
                return true;
            }
        }
    } else if (obj == m_emptyHint) {
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto* e = static_cast<QDragMoveEvent*>(event);
            if (hasAppFile(e->mimeData())) {
                e->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto* e = static_cast<QDropEvent*>(event);
            if (hasAppFile(e->mimeData())) {
                handleAppFileDrop(e->mimeData());
                e->acceptProposedAction();
                return true;
            }
        }
    } else if (obj == m_titleBar) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* e = static_cast<QMouseEvent*>(event);
            if (e->button() == Qt::LeftButton && windowHandle()) {
                qInfo() << "标题栏：开始拖动窗口";
                windowHandle()->startSystemMove();  // 系统接管拖动（支持贴边/多屏）
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            qInfo() << "标题栏：双击切换最大化";
            toggleMaximize();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::toggleMaximize() {
    if (isMaximized()) {
        qInfo() << "标题栏：还原窗口";
        showNormal();
        if (m_maxBtn) {
            m_maxBtn->setText("□");
        }
    } else {
        qInfo() << "标题栏：最大化窗口";
        showMaximized();
        if (m_maxBtn) {
            m_maxBtn->setText("❐");
        }
    }
}

#ifdef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result) {
    // 只处理通用窗口消息；Qt 还会传其它类型（如 dispatcher 消息），
    // 若不加区分地把非 MSG 数据当 MSG 解析会越界读取导致闪退
    if (eventType != "windows_generic_MSG") {
        return QMainWindow::nativeEvent(eventType, message, result);
    }
    MSG* msg = static_cast<MSG*>(message);
    if (msg->message == WM_NCHITTEST) {
        // 无边框窗口仍支持鼠标在边缘拖拽调整大小
        constexpr int border = 6;
        RECT rect;
        GetWindowRect(msg->hwnd, &rect);
        const int x = GET_X_LPARAM(msg->lParam);
        const int y = GET_Y_LPARAM(msg->lParam);
        const bool left = x >= rect.left && x < rect.left + border;
        const bool right = x > rect.right - border && x <= rect.right;
        const bool top = y >= rect.top && y < rect.top + border;
        const bool bottom = y > rect.bottom - border && y <= rect.bottom;
        if (top && left) {
            *result = HTTOPLEFT;
        } else if (top && right) {
            *result = HTTOPRIGHT;
        } else if (bottom && left) {
            *result = HTBOTTOMLEFT;
        } else if (bottom && right) {
            *result = HTBOTTOMRIGHT;
        } else if (left) {
            *result = HTLEFT;
        } else if (right) {
            *result = HTRIGHT;
        } else if (top) {
            *result = HTTOP;
        } else if (bottom) {
            *result = HTBOTTOM;
        } else {
            return false;
        }
        return true;
    }
    if (msg->message == WM_GETMINMAXINFO) {
        // 最大化时限制到工作区（避免无边框窗口遮挡任务栏）
        auto* mmi = reinterpret_cast<MINMAXINFO*>(msg->lParam);
        RECT work{};
        if (!SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0) ||
            (work.right - work.left <= 0) || (work.bottom - work.top <= 0)) {
            work = {0, 0, 1920, 1080};  // 取不到工作区时的兜底，避免 0 尺寸
        }
        if (HMONITOR hmon = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST)) {
            MONITORINFO mi{ sizeof(MONITORINFO) };
            if (GetMonitorInfo(hmon, &mi)) {
                work = mi.rcWork;
            }
        }
        mmi->ptMaxPosition.x = work.left;
        mmi->ptMaxPosition.y = work.top;
        mmi->ptMaxSize.x = work.right - work.left;
        mmi->ptMaxSize.y = work.bottom - work.top;
        return true;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}
#endif

void MainWindow::onGridContextMenu(const QPoint& pos) {
    auto* w = qobject_cast<QWidget*>(sender());
    QMenu menu(this);
    QAction* addAction = menu.addAction("＋ 添加收藏");
    QAction* addAppAction = menu.addAction("＋ 添加应用");
    connect(addAction, &QAction::triggered, this, &MainWindow::onAddBookmark);
    connect(addAppAction, &QAction::triggered, this, &MainWindow::onAddApp);
    menu.exec((w ? w : m_gridContainer)->mapToGlobal(pos));
}

void MainWindow::reorderBookmark(int id, int targetIndex) {
    // 排序仅在单个分组视图下有意义（“全部”视图跨分组，sort_order 语义不清）
    if (currentGroupId() < 0) {
        return;
    }
    int srcIndex = -1;
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        if (m_bookmarks[i].id == id) {
            srcIndex = i;
            break;
        }
    }
    if (srcIndex < 0) {
        return;
    }
    if (targetIndex > srcIndex) {
        --targetIndex;  // 移除后索引左移
    }
    targetIndex = qBound(0, targetIndex, m_bookmarks.size());
    Bookmark moved = m_bookmarks.takeAt(srcIndex);
    m_bookmarks.insert(targetIndex, moved);
    auto& repo = BookmarkRepository::instance();
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        repo.updateBookmarkOrder(m_bookmarks[i].id, i + 1);
    }
    refreshBookmarks();
}

void MainWindow::addAppFromCommand(const QString& command, const QString& name) {
    if (command.trimmed().isEmpty()) {
        return;
    }
    Bookmark b;
    b.groupId = currentGroupId() < 0 ? 0 : currentGroupId();
    b.type = "app";
    b.url = command.trimmed();
    b.name = name.trimmed();
    if (b.name.isEmpty()) {
        b.name = QFileInfo(BrowserLauncher::exePathFromCommand(b.url)).completeBaseName();
    }
    const int id = BookmarkRepository::instance().addBookmark(b);
    if (id > 0) {
        saveAppIcon(id, BrowserLauncher::exePathFromCommand(b.url));
    }
    refreshBookmarks();
}

void MainWindow::handleAppFileDrop(const QMimeData* mime) {
    if (!hasAppFile(mime)) {
        return;
    }
    for (const QUrl& url : mime->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        const QString p = url.toLocalFile();
        if (p.endsWith(".exe", Qt::CaseInsensitive)) {
            addAppFromCommand(p);
        } else if (p.endsWith(".lnk", Qt::CaseInsensitive)) {
            const QString target = resolveShortcutTarget(p);
            addAppFromCommand(target.isEmpty() ? p : target);
        }
    }
}

void MainWindow::saveAppIcon(int id, const QString& exePath) {
    if (exePath.isEmpty()) {
        return;
    }
    const QIcon icon = BrowserLauncher::iconForPath(exePath);
    if (icon.isNull()) {
        return;
    }
    const QPixmap pm = icon.pixmap(48, 48);
    if (pm.isNull()) {
        return;
    }
    const QString dirPath = QDir(AppConfig::instance().dataDir()).filePath("icons");
    QDir().mkpath(dirPath);
    const QString localPath = QDir(dirPath).filePath(QString("app_%1.png").arg(id));
    if (pm.save(localPath, "PNG")) {
        BookmarkRepository::instance().updateIcon(id, localPath);
    }
}

QString MainWindow::resolveShortcutTarget(const QString& lnkPath) {
#ifdef Q_OS_WIN
    const HRESULT init = CoInitialize(nullptr);
    QString result;
    IShellLinkW* link = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(ShellLink), nullptr, CLSCTX_INPROC_SERVER,
                                   __uuidof(IShellLinkW), reinterpret_cast<void**>(&link)))) {
        IPersistFile* file = nullptr;
        if (SUCCEEDED(link->QueryInterface(__uuidof(IPersistFile),
                                           reinterpret_cast<void**>(&file)))) {
            if (SUCCEEDED(file->Load(reinterpret_cast<const wchar_t*>(lnkPath.utf16()), STGM_READ))) {
                wchar_t target[MAX_PATH] = {};
                wchar_t args[MAX_PATH * 4] = {};
                if (SUCCEEDED(link->GetPath(target, MAX_PATH, nullptr, 0))) {
                    QString cmd = QString::fromWCharArray(target);
                    if (SUCCEEDED(link->GetArguments(args, MAX_PATH * 4)) && args[0] != L'\0') {
                        cmd += " " + QString::fromWCharArray(args);
                    }
                    result = cmd;
                }
            }
            file->Release();
        }
        link->Release();
    }
    if (SUCCEEDED(init)) {
        CoUninitialize();
    }
    return result;
#else
    Q_UNUSED(lnkPath);
    return QString();
#endif
}

void MainWindow::onOpenSettings() {
    SettingsDialog dlg(this);
    connect(&dlg, &SettingsDialog::dataRestored, this, &MainWindow::refreshGroups);
    connect(&dlg, &SettingsDialog::themeChanged, this, [this] { applyStyle(); });
    connect(&dlg, &SettingsDialog::backgroundChanged, this, [this] { applyStyle(); });
    // 实时预览：选择主题/背景/标题栏透明度时立即刷新，但不写入配置
    connect(&dlg, &SettingsDialog::previewChanged, this,
            [this](bool dark, const QString& bg, int op) { applyStyle(dark, bg, op); });
    dlg.exec();
    // 关闭后恢复为已保存的配置（取消时撤消预览；确定时配置已更新，等价幂等）
    applyStyle();
}

void MainWindow::onIconFetched(int id, const QString& path) {
    BookmarkRepository::instance().updateIcon(id, path);
    refreshBookmarks();
}

void MainWindow::setSelectionMode(bool enabled) {
    m_selectionMode = enabled;
    if (!enabled) {
        m_selectedIds.clear();
    }
    if (m_selectModeBtn && m_selectModeBtn->isChecked() != enabled) {
        m_selectModeBtn->setChecked(enabled);
    }
    updateBatchBar();
    refreshBookmarks();
}

void MainWindow::updateBatchBar() {
    if (m_batchBar) {
        m_batchBar->setVisible(m_selectionMode);
    }
    if (m_batchInfoLabel) {
        m_batchInfoLabel->setText(QString("已选 %1 项").arg(m_selectedIds.size()));
    }
    if (m_batchDeleteBtn) {
        m_batchDeleteBtn->setEnabled(!m_selectedIds.isEmpty());
    }
    if (m_batchMoveBtn) {
        m_batchMoveBtn->setEnabled(!m_selectedIds.isEmpty());
    }
}

void MainWindow::onToggleSelectionMode() {
    setSelectionMode(m_selectModeBtn->isChecked());
}

void MainWindow::onSelectionToggled(int id, bool selected) {
    if (selected) {
        m_selectedIds.insert(id);
    } else {
        m_selectedIds.remove(id);
    }
    updateBatchBar();
}

void MainWindow::onBatchSelectAll() {
    for (const Bookmark& b : m_bookmarks) {
        m_selectedIds.insert(b.id);
    }
    updateBatchBar();
    refreshBookmarks();
}

void MainWindow::onBatchDelete() {
    if (m_selectedIds.isEmpty()) {
        return;
    }
    const int n = m_selectedIds.size();
    const auto ret = QMessageBox::question(
        this, "批量删除", QString("确定删除选中的 %1 个收藏吗？").arg(n),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret != QMessageBox::Yes) {
        return;
    }
    BookmarkRepository::instance().deleteBookmarks(m_selectedIds.values());
    m_selectedIds.clear();
    updateBatchBar();
    refreshBookmarks();
}

void MainWindow::onBatchMove() {
    if (m_selectedIds.isEmpty()) {
        return;
    }
    if (m_groups.isEmpty()) {
        QMessageBox::information(this, "提示", "请先创建一个分组。");
        return;
    }
    QStringList names;
    for (const Group& g : m_groups) {
        names << g.name;
    }
    bool ok = false;
    const QString choice =
        QInputDialog::getItem(this, "移动到分组", "目标分组：", names, 0, false, &ok);
    if (!ok) {
        return;
    }
    const int idx = names.indexOf(choice);
    if (idx < 0 || idx >= m_groups.size()) {
        return;
    }
    BookmarkRepository::instance().moveBookmarks(m_selectedIds.values(), m_groups[idx].id);
    m_selectedIds.clear();
    updateBatchBar();
    refreshBookmarks();
}
