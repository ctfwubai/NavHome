#pragma once

#include <QList>
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

private:
    BookmarkRepository() = default;
    QSqlDatabase m_db;
};
