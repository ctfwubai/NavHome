#pragma once

#include <QString>

// 收藏数据结构，对应数据库 bookmarks 表
struct Bookmark {
    int id = -1;           // 主键（自增）
    int groupId = 0;       // 所属分组 id；0 表示“未分组”
    QString name;          // 名称
    QString url;           // 网址（type=url 时为网址；type=app 时为程序路径/命令行）
    QString icon;          // 图标：本地文件路径或空（使用占位图标）
    QString username;      // 账号（明文，内存中；数据库明文存储）
    QString password;      // 密码（明文，内存中；数据库经 DPAPI 加密存储）
    QString type = "url";  // 类型：url=网页收藏，app=应用程序
    int sortOrder = 0;     // 排序值
    QString createdAt;     // 创建时间（ISO 格式）
    QString updatedAt;     // 更新时间（ISO 格式）
};
