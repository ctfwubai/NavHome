#pragma once

#include <QList>
#include <QSet>
#include <QSqlDatabase>

#include "models/Group.h"
#include "models/Bookmark.h"

// 收藏与分组的数据访问层：封装所有 SQLite 读写（对应需求 3.4 / 3.5）
class BookmarkRepository {
public:
    static BookmarkRepository& instance();

    // 绑定数据库连接
    void setDatabase(QSqlDatabase db);

    // —— 分组 ——
    QList<Group> groups();
    int addGroup(const QString& name);
    bool renameGroup(int id, const QString& name);
    // 删除分组：moveToUngrouped=true 时组内收藏移至“未分组”，否则一并删除
    bool deleteGroup(int id, bool moveToUngrouped);
    bool updateGroupOrder(int id, int order);

    // —— 收藏 ——
    QList<Bookmark> bookmarks(int groupId);   // groupId < 0 表示全部
    QList<Bookmark> allBookmarks();
    QList<Bookmark> search(const QString& keyword);
    int addBookmark(const Bookmark& bookmark);
    bool updateBookmark(const Bookmark& bookmark);
    bool deleteBookmark(int id);
    bool deleteBookmarks(const QList<int>& ids);
    bool moveBookmarks(const QList<int>& ids, int targetGroupId);
    bool updateBookmarkOrder(int id, int order);
    bool updateIcon(int id, const QString& icon);

    // —— 文件夹 ——
    // 创建文件夹（type=folder 的收藏），返回其 id；name 为空则用默认名
    int addFolder(int groupId, const QString& name = QString());
    // 解散文件夹：成员回到网格，文件夹本身删除
    bool deleteFolder(int folderId);
    // 将收藏收纳进文件夹
    bool addItemToFolder(int folderId, int bookmarkId);
    bool removeItemFromFolder(int folderId, int bookmarkId);
    // 文件夹成员列表（按 sort_order）
    QList<Bookmark> folderItems(int folderId);
    // 所有已被收纳的收藏 id（主网格据此隐藏它们）
    QSet<int> folderMemberIds();
    bool isFolderMember(int bookmarkId);

private:
    BookmarkRepository() = default;
    QSqlDatabase m_db;
};
