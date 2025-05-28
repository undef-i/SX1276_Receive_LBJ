//
// Created by FLN1021 on 2024/2/8.
//

#include <cstdint>
#include "aPreferences.h"

aPreferences::aPreferences() : pref{}, have_pref(false), overflow(false), lines(0), ret_lines(0), ids(0), ns_name{} {

}

bool aPreferences::begin(const char *name, bool read_only) {
    Serial.printf("[NVS] 开始初始化命名空间: %s, 分区: %s\n", name, partition_name);
    
    // 初始化指定分区
    esp_err_t err = nvs_flash_init_partition(partition_name);
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        Serial.println("[NVS] 分区需要擦除重新格式化");
        // 擦除并重新初始化
        err = nvs_flash_erase_partition(partition_name);
        if (err != ESP_OK) {
            Serial.printf("[NVS] 分区擦除失败: %s\n", esp_err_to_name(err));
            return false;
        }
        err = nvs_flash_init_partition(partition_name);
    }
    if (err != ESP_OK) {
        Serial.printf("[NVS] 分区初始化失败: %s\n", esp_err_to_name(err));
        return false;
    }
    
    // 尝试打开命名空间
    if (!pref.begin(name, read_only, partition_name)) {
        Serial.printf("[NVS] 初始化失败: 无法打开命名空间\n");
        return false;
    }
    have_pref = true;
    ns_name = name;

    if (pref.isKey("ids")) {
        ids = pref.getUInt("ids");
        Serial.printf("[NVS] 读取已存储ID: %u\n", ids);
    }
    if (pref.isKey("lines")) {
        lines = pref.getUShort("lines");
        Serial.printf("[NVS] 读取已存储行数: %u\n", lines);
    }
    ret_lines = lines;

    char buf[8];
    sprintf(buf, "I%04d", lines + 1);
    if (pref.isKey(buf)) {
        overflow = true;
        Serial.printf("[NVS] 检测到存储溢出\n");
    }

    getStats();
    Serial.println("[NVS] 初始化完成");
    return true;
}

bool aPreferences::append(lbj_data lbj, rx_info rx, float volt, float temp) {
    if (!have_pref)
        return false;
    if (lbj.type == -1)
        return false;
    /* Standard format of cache:
     * 条目数,电压,系统时间,温度,日期,时间,type,train,direction,speed,position,time,info2_hex,loco_type,lbj_class,loco,route,
     * route_utf8,pos_lon_deg,pos_lon_min,pos_lat_deg,pos_lat_min,pos_lon,pos_lat,rssi,fer,ppm,id
     */
    String line;
    char buffer[256];
    struct tm now{};
    getLocalTime(&now, 1);

    if (lines > PREF_MAX_LINES) {
        // todo: add countermeasures (cycling).
        lines = 0;
        overflow = true;
    }
    sprintf(buffer, "%04d,%1.2f,%lld,", lines, volt, esp_timer_get_time());
    line += buffer;
    sprintf(buffer, "%.2f,%d-%02d-%02d,%02d:%02d:%02d,", temp, now.tm_year + 1900, now.tm_mon + 1,
            now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);
    line += buffer;
    //type,train,direction,speed,position,time,info2_hex,loco_type,
    sprintf(buffer, "%d,%s,%d,%s,%s,%s,%s,%s,", lbj.type, lbj.train, lbj.direction, lbj.speed, lbj.position, lbj.time,
            lbj.info2_hex.c_str(), lbj.loco_type.c_str());
    line += buffer;
    // lbj_class,loco,route,route_utf8,pos_lon_deg,pos_lon_min,pos_lat_deg,pos_lat_min,pos_lon,pos_lat
    sprintf(buffer, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,", lbj.lbj_class, lbj.loco, lbj.route, lbj.route_utf8,
            lbj.pos_lon_deg, lbj.pos_lon_min, lbj.pos_lat_deg, lbj.pos_lat_min, lbj.pos_lon, lbj.pos_lat);
    line += buffer;
    // rssi,fer,ppm,ids
    sprintf(buffer, "%.2f,%.2f,%.2f,%u,", rx.rssi, rx.fer, rx.ppm, ids);
    line += buffer;
    // epi
    line += lbj.epi;
    // line += ',';
    // // u8char_len
    // for (int i = 0; i < sizeof lbj.u8char_len / sizeof (int); ++i) {
    //     sprintf(buffer, "%d",lbj.u8char_len[i]);
    //     line += buffer;
    // }

    sprintf(buffer, "I%04d", lines);
    pref.putString(buffer, line);
    lines++;
    pref.putUShort("lines", lines);
    ids++;
    pref.putUInt("ids", ids);
    return true;
}

bool
aPreferences::retrieve(lbj_data *lbj, rx_info *rx, String *time_str, uint16_t *line_num, uint32_t *id, float *temp,
                        int8_t bias) {
    if (!have_pref) {
        Serial.println("[NVS] Preferences not initialized");
        return false;
    }

    ret_lines += bias;
    if (overflow) {
        if (ret_lines < 0)
            ret_lines = PREF_MAX_LINES;
        if (ret_lines > PREF_MAX_LINES)
            ret_lines = 0;
    } else {
        if (ret_lines < 0)
            ret_lines = lines - 1;
        if (ret_lines >= lines)
            ret_lines = 0;
    }

    char buf[8];
    sprintf(buf, "I%04d", ret_lines);
    
    if (!pref.isKey(buf)) {
        Serial.printf("[NVS] Key %s not found\n", buf);
        return false;
    }

    Serial.printf("[NVS] Retrieving line %d\n", ret_lines);
    String line = pref.getString(buf);
    if (line.isEmpty()) {
        Serial.println("[NVS] Retrieved empty line");
        return false;
    }
    Serial.printf("[NVS] Retrieved data: %s\n", line.c_str());

    // Tokenize
    String tokens[29];
    for (size_t i = 0, c = 0; i < line.length(); i++) {
        if (line[i] == ',') {
            c++;
            if (c >= 29) break; // 防止超出数组边界
            continue;
        }
        if (c < 29)
            tokens[c] += line[i];
    }

    try {
        // 基本数据转换
        if (line_num && !tokens[0].isEmpty()) *line_num = std::stoi(tokens[0].c_str());
        if (temp && !tokens[3].isEmpty()) *temp = std::stof(tokens[3].c_str());
        
        // 时间信息
        if (time_str && !tokens[4].isEmpty() && !tokens[5].isEmpty()) {
            *time_str = tokens[4] + " " + tokens[5];
        }

        // LBJ数据转换
        if (lbj) {
            if (!tokens[6].isEmpty()) lbj->type = (int8_t) std::stoi(tokens[6].c_str());
            if (!tokens[7].isEmpty()) strncpy(lbj->train, tokens[7].c_str(), sizeof(lbj->train) - 1); lbj->train[sizeof(lbj->train) - 1] = '\0';
            if (!tokens[8].isEmpty()) lbj->direction = (int8_t) std::stoi(tokens[8].c_str());
            if (!tokens[9].isEmpty()) strncpy(lbj->speed, tokens[9].c_str(), sizeof(lbj->speed) - 1); lbj->speed[sizeof(lbj->speed) - 1] = '\0';
            if (!tokens[10].isEmpty()) strncpy(lbj->position, tokens[10].c_str(), sizeof(lbj->position) - 1); lbj->position[sizeof(lbj->position) - 1] = '\0';
            if (!tokens[11].isEmpty()) strncpy(lbj->time, tokens[11].c_str(), sizeof(lbj->time) - 1); lbj->time[sizeof(lbj->time) - 1] = '\0';
            if (!tokens[12].isEmpty()) lbj->info2_hex = tokens[12];
            if (!tokens[13].isEmpty()) lbj->loco_type = tokens[13];

            if (!tokens[14].isEmpty()) strncpy(lbj->lbj_class, tokens[14].c_str(), sizeof(lbj->lbj_class) - 1); lbj->lbj_class[sizeof(lbj->lbj_class) - 1] = '\0';
            if (!tokens[15].isEmpty()) strncpy(lbj->loco, tokens[15].c_str(), sizeof(lbj->loco) - 1); lbj->loco[sizeof(lbj->loco) - 1] = '\0';
            if (!tokens[16].isEmpty()) strncpy(lbj->route, tokens[16].c_str(), sizeof(lbj->route) - 1); lbj->route[sizeof(lbj->route) - 1] = '\0';
            if (!tokens[17].isEmpty()) strncpy(lbj->route_utf8, tokens[17].c_str(), sizeof(lbj->route_utf8) - 1); lbj->route_utf8[sizeof(lbj->route_utf8) - 1] = '\0';
            if (!tokens[18].isEmpty()) strncpy(lbj->pos_lon_deg, tokens[18].c_str(), sizeof(lbj->pos_lon_deg) - 1); lbj->pos_lon_deg[sizeof(lbj->pos_lon_deg) - 1] = '\0';
            if (!tokens[19].isEmpty()) strncpy(lbj->pos_lon_min, tokens[19].c_str(), sizeof(lbj->pos_lon_min) - 1); lbj->pos_lon_min[sizeof(lbj->pos_lon_min) - 1] = '\0';
            if (!tokens[20].isEmpty()) strncpy(lbj->pos_lat_deg, tokens[20].c_str(), sizeof(lbj->pos_lat_deg) - 1); lbj->pos_lat_deg[sizeof(lbj->pos_lat_deg) - 1] = '\0';
            if (!tokens[21].isEmpty()) strncpy(lbj->pos_lat_min, tokens[21].c_str(), sizeof(lbj->pos_lat_min) - 1); lbj->pos_lat_min[sizeof(lbj->pos_lat_min) - 1] = '\0';
            if (!tokens[22].isEmpty()) strncpy(lbj->pos_lon, tokens[22].c_str(), sizeof(lbj->pos_lon) - 1); lbj->pos_lon[sizeof(lbj->pos_lon) - 1] = '\0';
            if (!tokens[23].isEmpty()) strncpy(lbj->pos_lat, tokens[23].c_str(), sizeof(lbj->pos_lat) - 1); lbj->pos_lat[sizeof(lbj->pos_lat) - 1] = '\0';
            if (!tokens[28].isEmpty()) lbj->epi = tokens[28];
        }

        if (rx) {
            if (!tokens[24].isEmpty()) rx->rssi = std::stof(tokens[24].c_str());
            if (!tokens[25].isEmpty()) rx->fer = std::stof(tokens[25].c_str());
            if (!tokens[26].isEmpty()) rx->ppm = std::stof(tokens[26].c_str());
        }
        
        if (id && !tokens[27].isEmpty()) *id = std::stoul(tokens[27].c_str());

    } catch (const std::exception& e) {
        Serial.printf("[NVS] Data conversion error: %s\n", e.what());
        return false;
    }

    if (rx) {
        Serial.printf("[NVS] Data retrieved successfully: line=%d, rssi=%.2f, fer=%.2f, ppm=%.2f\n",
                    ret_lines, rx->rssi, rx->fer, rx->ppm);
    } else {
        Serial.printf("[NVS] Data retrieved successfully: line=%d\n", ret_lines);
    }
    return true;
}

bool aPreferences::clearKeys() {
    uint32_t id = pref.getUInt("ids");
    if (!pref.clear())
        return false;
    overflow = false;
    lines = 0;
    pref.putUShort("lines", lines);
    ret_lines = 0;
    pref.putUInt("ids", id);
    return true;
}

void aPreferences::toLatest(int8_t bias) {
    ret_lines = lines + bias;
}

void aPreferences::getStats() {
    if (!have_pref) {
        Serial.println("[NVS] 无法获取统计信息: 存储未初始化");
        return;
    }
    nvs_stats_t stats;
    if (nvs_get_stats(partition_name, &stats) != ESP_OK) {
        Serial.println("[NVS] 获取统计信息失败");
        return;
    }
    
    float usage_percent = (float)stats.used_entries / stats.total_entries * 100;
    Serial.printf("[NVS] 分区使用情况 (%s):\n", partition_name);
    Serial.printf("  - 已用条目: %d\n  - 空闲条目: %d\n  - 总条目数: %d\n  - 使用率: %.1f%%\n",
                 stats.used_entries, stats.free_entries, stats.total_entries, usage_percent);
    
    nvs_handle_t handle;
    if (nvs_open_from_partition(partition_name, ns_name, NVS_READONLY, &handle) != ESP_OK) {
        Serial.println("[NVS] 打开命名空间失败");
        return;
    }
    size_t used_entries;
    nvs_get_used_entry_count(handle, &used_entries);
    nvs_close(handle);
    Serial.printf("[NVS] 命名空间 %s 使用的条目数: %d\n", ns_name, used_entries);
    Serial.printf("[NVS] 当前行数: %d, ID: %d\n", lines, ids);
}

bool aPreferences::isLatest(int8_t bias) const {
    if (ret_lines == lines + bias)
        return true;
    else
        return false;
}

bool aPreferences::retrieve(String *str_array, uint32_t arr_size, int8_t bias) {
    if (!have_pref)
        return false;
    ret_lines += bias;
    if (overflow) {
        if (ret_lines < 0)
            ret_lines = PREF_MAX_LINES;
        if (ret_lines > PREF_MAX_LINES)
            ret_lines = 0;
    } else {
        if (ret_lines < 0)
            ret_lines = lines - 1;
        if (ret_lines == lines)
            ret_lines = 0;
    }

    char buf[8];
    sprintf(buf, "I%04d", ret_lines);
    if (!pref.isKey(buf))
        return false;
    Serial.printf("[D] ret_line = %d\n", ret_lines);
    String line = pref.getString(buf);
    Serial.printf("[D] %s \n", line.c_str());
    Serial.printf("arr size = %d\n",arr_size);
    // Tokenize
    // String tokens[28];
    for (size_t i = 0, c = 0; i < line.length(); i++) {
        if (c >= arr_size)
            break;
        if (line[i] == ',') {
            c++;
            continue;
        }
        str_array[c] += line[i];
    }
    return true;
}

int32_t aPreferences::getRetLines() {
    return ret_lines;
}

void aPreferences::setRetLines(int32_t line) {
    ret_lines = line;
}

bool aPreferences::clearData() {
    if (!have_pref)
        return false;
    // 清除所有存储数据
    bool success = clearKeys();
    if (success) {
        write_count = 0;
        error_count = 0;
    }
    return success;
}

uint32_t aPreferences::getTotalSpace() {
    nvs_stats_t stats;
    if (nvs_get_stats(partition_name, &stats) != ESP_OK) {
        Serial.println("[NVS] 获取存储统计失败");
        return 0;
    }
    // 每个条目假设平均100字节
    return stats.total_entries * 100;
}

uint32_t aPreferences::getUsedSpace() {
    nvs_stats_t stats;
    if (nvs_get_stats(partition_name, &stats) != ESP_OK) {
        Serial.println("[NVS] 获取存储统计失败");
        return 0;
    }
    // 每个条目假设平均100字节
    return stats.used_entries * 100;
}

uint32_t aPreferences::getFileCount() {
    // 返回已保存的数据记录数
    return lines;
}

uint32_t aPreferences::getWriteCount() {
    return write_count;
}

uint32_t aPreferences::getErrorCount() {
    return error_count;
}

uint8_t aPreferences::getStorageHealth() {
    nvs_stats_t stats;
    if (nvs_get_stats(partition_name, &stats) != ESP_OK) {
        return 0;
    }
    
    // 计算健康度
    uint8_t health = 100;
    
    // 根据使用率降低健康度
    float usage_percent = (float)stats.used_entries / stats.total_entries * 100;
    if (usage_percent > 90) {
        health -= 30;
    } else if (usage_percent > 80) {
        health -= 20;
    } else if (usage_percent > 70) {
        health -= 10;
    }
    
    // 根据错误率降低健康度
    if (write_count > 0) {
        float error_rate = (float)error_count / write_count * 100;
        if (error_rate > 5) {
            health -= 30;
        } else if (error_rate > 2) {
            health -= 20;
        } else if (error_rate > 1) {
            health -= 10;
        }
    }
    
    return health;
}

uint32_t aPreferences::getStorageLimit() {
    return storage_limit;
}

void aPreferences::setStorageLimit(uint32_t limit) {
    storage_limit = limit;
}
