#pragma once

#include <QString>

// 数据备份与恢复（对应需求 3.8）
class BackupManager {
public:
    // 导入模式
    enum ImportMode {
        Overwrite,   // 覆盖现有数据
        Merge,       // 合并（保留现有，追加备份数据）
    };

    // 导出收藏与分组到 JSON 文件；成功返回 true，否则 error 描述错误
    static bool exportData(const QString& filePath, QString* error = nullptr);

    // 从 JSON 文件恢复；成功返回 true
    static bool importData(const QString& filePath, ImportMode mode, QString* error = nullptr);
};
