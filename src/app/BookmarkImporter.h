#pragma once

#include <QString>

// 浏览器书签 HTML 导入（Netscape Bookmark File Format，Chrome/Edge/Firefox 通用）
// 对应需求：从浏览器导出的书签文件批量导入收藏
class BookmarkImporter {
public:
    struct Result {
        int groupsCreated = 0;      // 新建分组数（同名分组自动复用）
        int bookmarksImported = 0;  // 导入收藏数
    };

    // 解析并导入书签 HTML 文件（整体在一个事务内完成）
    // 同名分组自动复用，未归入文件夹的书签进入“未分组”（group_id = 0）
    static bool import(const QString& filePath, Result* result, QString* error = nullptr);

    // 直接读取 Chrome/Edge 本地书签 JSON（无需手动导出 HTML）
    static bool importChromeBookmarks(const QString& bookmarksJsonPath, Result* result, QString* error = nullptr);

    // 从文件夹批量导入（递归扫描其中的 .html/.htm 书签文件，合并导入）
    static bool importFolder(const QString& dirPath, Result* result, QString* error = nullptr);

    // 检测本机浏览器书签文件路径，不存在则返回空串
    static QString chromeBookmarksPath();
    static QString edgeBookmarksPath();
};
