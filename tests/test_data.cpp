// 数据层冒烟测试：验证分组 / 收藏 CRUD 与备份恢复逻辑
#include <QCoreApplication>
#include <QFile>
#include <QDebug>

#include "app/Database.h"
#include "app/BackupManager.h"
#include "models/BookmarkRepository.h"

static int failures = 0;

#define CHECK(cond, msg)                                  \
    do {                                                  \
        if (cond) {                                       \
            qInfo() << "  [PASS]" << msg;                 \
        } else {                                          \
            qInfo() << "  [FAIL]" << msg;                 \
            ++failures;                                   \
        }                                                 \
    } while (0)

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    const QString dbPath = "test_data.db";
    const QString backupPath = "test_backup.json";
    QFile::remove(dbPath);
    QFile::remove(backupPath);

    if (!Database::instance().open(dbPath)) {
        qCritical() << "数据库打开失败";
        return 1;
    }
    BookmarkRepository::instance().setDatabase(Database::instance().database());

    qInfo() << "== 分组 ==";
    const int g1 = BookmarkRepository::instance().addGroup("工作");
    const int g2 = BookmarkRepository::instance().addGroup("学习");
    CHECK(g1 > 0 && g2 > 0, "新增分组");
    CHECK(BookmarkRepository::instance().groups().size() == 2, "分组数量 = 2");

    qInfo() << "== 收藏 ==";
    Bookmark b;
    b.name = "GitHub";
    b.url = "https://github.com";
    b.groupId = g1;
    const int id1 = BookmarkRepository::instance().addBookmark(b);
    b.name = "知乎";
    b.url = "https://zhihu.com";
    b.groupId = g2;
    const int id2 = BookmarkRepository::instance().addBookmark(b);
    CHECK(id1 > 0 && id2 > 0, "新增收藏");
    CHECK(BookmarkRepository::instance().bookmarks(g1).size() == 1, "分组 g1 收藏数 = 1");
    CHECK(BookmarkRepository::instance().allBookmarks().size() == 2, "全部收藏 = 2");
    CHECK(BookmarkRepository::instance().search("git").size() == 1, "搜索 'git' 命中 1 条");

    qInfo() << "== 更新 / 删除 ==";
    const QList<Bookmark> all = BookmarkRepository::instance().allBookmarks();
    for (Bookmark x : all) {
        if (x.id == id1) {
            x.name = "GitHub 代码托管";
            BookmarkRepository::instance().updateBookmark(x);
        }
    }
    CHECK(BookmarkRepository::instance().search("代码托管").size() == 1, "更新收藏生效");
    CHECK(BookmarkRepository::instance().deleteBookmark(id2), "删除收藏");
    CHECK(BookmarkRepository::instance().allBookmarks().size() == 1, "删除后剩余 1 条");

    qInfo() << "== 备份 / 恢复 ==";
    QString err;
    CHECK(BackupManager::exportData(backupPath, &err), "导出备份");

    BookmarkRepository::instance().deleteBookmark(id1);
    BookmarkRepository::instance().deleteGroup(g1, true);
    BookmarkRepository::instance().deleteGroup(g2, true);

    CHECK(BackupManager::importData(backupPath, BackupManager::Overwrite, &err), "覆盖恢复");
    CHECK(BookmarkRepository::instance().allBookmarks().size() == 1, "覆盖恢复后收藏数 = 1");
    CHECK(BookmarkRepository::instance().groups().size() == 2, "覆盖恢复后分组数 = 2");

    CHECK(BackupManager::importData(backupPath, BackupManager::Merge, &err), "合并恢复");
    CHECK(BookmarkRepository::instance().allBookmarks().size() == 2, "合并恢复后收藏数 = 2");

    qInfo() << "== 文件夹备份 / 恢复 ==";
    {
        // 基于当前 2 条收藏创建文件夹并收纳，验证 type 与 folder_items 能随备份完整往返
        const QList<Bookmark> cur = BookmarkRepository::instance().allBookmarks();
        CHECK(cur.size() >= 2, "前置：至少有 2 条收藏用于建文件夹");
        const int aId = cur[0].id;
        const int bId = cur[1].id;
        const int fid = BookmarkRepository::instance().addFolder(cur[0].groupId, "测试文件夹");
        BookmarkRepository::instance().addItemToFolder(fid, aId);
        BookmarkRepository::instance().addItemToFolder(fid, bId);
        CHECK(BookmarkRepository::instance().folderItems(fid).size() == 2, "文件夹成员 = 2");

        const QString fbPath = "test_folder_backup.json";
        QFile::remove(fbPath);
        CHECK(BackupManager::exportData(fbPath, &err), "导出含文件夹的备份");

        BookmarkRepository::instance().deleteFolder(fid);
        BookmarkRepository::instance().deleteBookmark(aId);
        BookmarkRepository::instance().deleteBookmark(bId);
        CHECK(BackupManager::importData(fbPath, BackupManager::Overwrite, &err), "覆盖恢复含文件夹备份");

        int folderCount = 0;
        int restoredFolderId = -1;
        const QList<Bookmark> restored = BookmarkRepository::instance().allBookmarks();
        for (const Bookmark& x : restored) {
            if (x.type == "folder") {
                ++folderCount;
                restoredFolderId = x.id;
            }
        }
        CHECK(folderCount == 1, "恢复后文件夹数量 = 1（type 字段保留）");
        CHECK(restoredFolderId > 0 &&
                  BookmarkRepository::instance().folderItems(restoredFolderId).size() == 2,
              "恢复后文件夹成员 = 2（folder_items 保留）");
    }

    qInfo() << (failures == 0 ? "== 全部通过 ==" : "== 存在失败 ==");
    return failures == 0 ? 0 : 1;
}
