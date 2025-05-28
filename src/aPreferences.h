//
// Created by FLN1021 on 2024/2/8.
//

#ifndef PAGER_RECEIVE_APREFERENCES_H
#define PAGER_RECEIVE_APREFERENCES_H

#include "networks.hpp"
#include <Preferences.h>
#include <nvs_flash.h>
#include <cstdint>

const int PREF_MAX_LINES = 2500;

class aPreferences {
public:
    aPreferences();

    bool begin(const char *name, bool read_only);

    bool append(lbj_data lbj, rx_info rx, float volt, float temp);

    bool retrieve(lbj_data *lbj, rx_info *rx, String *time_str, uint16_t *line_num, uint32_t *id, float *temp,
                  int8_t bias);

    bool retrieve(String *str_array, uint32_t arr_size, int8_t bias);

    bool clearKeys();

    void toLatest(int8_t bias = -1);

    bool isLatest(int8_t bias = -1) const;

    void getStats();

    int32_t getRetLines();

    void setRetLines(int32_t line);

    // 存储管理相关接口
    bool clearData();  // 清除所有存储数据
    uint32_t getTotalSpace();  // 获取总存储空间
    uint32_t getUsedSpace();   // 获取已用存储空间
    uint32_t getFileCount();   // 获取文件数量
    uint32_t getWriteCount();  // 获取写入次数
    uint32_t getErrorCount();  // 获取错误次数
    uint8_t getStorageHealth();  // 获取存储健康度（百分比）
    uint32_t getStorageLimit();  // 获取存储限制大小
    void setStorageLimit(uint32_t limit);  // 设置存储限制大小

private:
    uint32_t write_count = 0;     // 写入计数
    uint32_t error_count = 0;     // 错误计数
    uint32_t storage_limit = 1024 * 1024;  // 默认1MB存储限制
    Preferences pref;
    const char *ns_name;
    bool have_pref;
    bool overflow;
    uint16_t lines;
    int32_t ret_lines; // defaults -1
    uint32_t ids;
    const char *partition_name = "nvs";
};


#endif //PAGER_RECEIVE_APREFERENCES_H
