#pragma once

#include <QMainWindow>
#include <QList>
#include <QSet>
#include <QPoint>

#include "models/Group.h"
#include "models/Bookmark.h"

class QListWidget;
class QLineEdit;
class QMimeData;
class QScrollArea;
class QLabel;
class QPushButton;
class FlowLayout;
class IconFetcher;

// 主窗口：左侧分组菜单 + 右侧收藏卡片网格（对应需求 3.3）
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // 依据当前主题设置全局样式表（含登录对话框等主窗口之前的界面）
    static void applyGlobalStyle();

protected:
    // 拦截分组列表 / 网格容器的拖拽释放，实现跨分组移动与排序
    bool eventFilter(QObject* obj, QEvent* event) override;
    // 无边框窗口的边缘调整大小与最大化适配
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private slots:
    void onAddGroup();
    void onRenameGroup();
    void onDeleteGroup();
    void onAddBookmark();
    void onAddApp();
    void onEditBookmark(int id);
    void onDeleteBookmark(int id);
    void onDeleteFolder(int folderId);
    void onGroupSelected();
    void onFilterChanged(const QString& text);
    void onOpenUrl(const QString& url);
    void onOpenApp(const QString& command);
    void onOpenSettings();
    void onIconFetched(int id, const QString& path);
    void onToggleSelectionMode();
    void onBatchDelete();
    void onBatchMove();
    void onBatchSelectAll();
    void onSelectionToggled(int id, bool selected);
    void onGridContextMenu(const QPoint& pos);

private:
    void buildUi();
    void applyStyle();
    void applyStyle(bool dark, const QString& bg, int titleBarOpacity);  // 预览用：用临时值刷新，不写配置
    void applyToolbarStyle(bool dark, bool hasBg);  // 顶部工具栏控件随背景半透明
    void toggleMaximize();
    static QString themeStylesheet(bool dark, const QString& leftPanelBg);
    void refreshGroups();
    void refreshBookmarks();
    void ensureBrowserConfigured();
    void setSelectionMode(bool enabled);
    void updateBatchBar();

    // 当前选中分组 id；-1 表示“全部”
    int currentGroupId() const;
    const Group* findGroup(int id) const;
    Bookmark* findBookmark(int id);
    void updateEmptyHint();
    void reorderBookmark(int id, int targetIndex);
    // 由程序路径/命令行创建应用卡片并提取图标（拖入 .exe/.lnk 与手动添加共用）
    void addAppFromCommand(const QString& command, const QString& name = QString());
    // 提取 exe 图标并保存为本地 PNG，写回 icon 字段
    void saveAppIcon(int id, const QString& exePath);
    // 解析 Windows 快捷方式（.lnk）目标命令行；失败返回空
    QString resolveShortcutTarget(const QString& lnkPath);
    // 处理文件拖放：为每个 .exe/.lnk 生成应用卡片
    void handleAppFileDrop(const QMimeData* mime);
    // 将两个收藏合并为一个文件夹（仅单分组视图）
    void createFolder(int aId, int bId);
    // 将收藏收纳进已有文件夹
    void addToFolder(int folderId, int bookmarkId);

    QList<Group> m_groups;
    QList<Bookmark> m_bookmarks;   // 当前视图展示的收藏

    QListWidget* m_groupList = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QLabel* m_emptyHint = nullptr;
    QWidget* m_centralWidget = nullptr;
    QWidget* m_leftPanel = nullptr;
    QPushButton* m_toggleSidebarBtn = nullptr;
    QWidget* m_titleBar = nullptr;
    QPushButton* m_minBtn = nullptr;
    QPushButton* m_maxBtn = nullptr;
    QPushButton* m_closeBtn = nullptr;
    QScrollArea* m_scrollArea = nullptr;
    QWidget* m_gridContainer = nullptr;
    FlowLayout* m_gridLayout = nullptr;
    IconFetcher* m_iconFetcher = nullptr;

    bool m_selectionMode = false;
    QSet<int> m_selectedIds;

    QPushButton* m_selectModeBtn = nullptr;
    QPushButton* m_settingsBtn = nullptr;
    QWidget* m_batchBar = nullptr;
    QLabel* m_batchInfoLabel = nullptr;
    QPushButton* m_batchDeleteBtn = nullptr;
    QPushButton* m_batchMoveBtn = nullptr;

    // 样式缓存：仅主题/背景变化时才重设全局样式表，标题栏透明度变化只刷标题栏
    bool m_styleInited = false;
    bool m_lastDark = false;
    QString m_lastBg;
};
