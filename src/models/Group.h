#pragma once

#include <QString>

// 分组数据结构，对应数据库 groups 表
struct Group {
    int id = -1;          // 主键（自增）
    QString name;         // 分组名
    int sortOrder = 0;    // 排序值，越小越靠前
};
