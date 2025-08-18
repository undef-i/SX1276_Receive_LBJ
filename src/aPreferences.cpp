//
// Created by FLN1021 on 2024/2/8.
//

#include <cstdint>
#include "aPreferences.h"

aPreferences::aPreferences() : pref{}, have_pref(false), overflow(false), lines(0), ret_lines(0), ids(0), ns_name{} {

}

bool aPreferences::begin(const char *name, bool read_only) {
    if (!pref.begin(name, read_only, partition_name))
        return false;
    have_pref = true;
    ns_name = name;

    if (pref.isKey("ids"))
        ids = pref.getUInt("ids");
    if (pref.isKey("lines"))
        lines = pref.getUShort("lines");
    ret_lines = lines;

    char buf[8];
    sprintf(buf, "I%04d", lines + 1);
    if (pref.isKey(buf))
        overflow = true;

    getStats();
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
    // if (ret_lines < 0)
    //     ret_lines = lines - 1;
    // if (ret_lines == lines && !pref.isKey(buf))
    //     ret_lines = 0;
    // if (ret_lines == lines)
    //     ret_lines--;
    // if (ret_lines + bias < 0 || ret_lines + bias == lines)
    //     return false;
    // ret_lines += bias;
    // char buf[8];
    // sprintf(buf, "I%04d", ret_lines + bias);
    if (!pref.isKey(buf))
        return false;
    // ret_lines += bias;
    Serial.printf("[D] ret_line = %d\n", ret_lines);
    String line = pref.getString(buf);
    Serial.printf("[D] %s \n", line.c_str());
    // Tokenize
    String tokens[29];
    for (size_t i = 0, c = 0; i < line.length(); i++) {
        if (line[i] == ',') {
            c++;
            continue;
        }
        if (c < 29)
            tokens[c] += line[i];
    }
    // sprintf(buffer, "%04d,%1.2f,%llu,", lines, volt, esp_timer_get_time());
    *line_num = std::stoi(tokens[0].c_str());
    // sprintf(buffer, "%.2f,%d-%02d-%02d,%02d:%02d:%02d,", temp, now.tm_year + 1900, now.tm_mon + 1,
    //         now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);
    *temp = std::stof(tokens[3].c_str());
    *time_str = tokens[4] + " " + tokens[5];

    // sprintf(buffer, "%d,%s,%d,%s,%s,%s,%s,%s,", lbj.type, lbj.train, lbj.direction, lbj.speed, lbj.position, lbj.time,
    //         lbj.info2_hex.c_str(), lbj.loco_type.c_str());
    lbj->type = (int8_t) std::stoi(tokens[6].c_str());
    tokens[7].toCharArray(lbj->train, sizeof lbj->train);
    lbj->direction = (int8_t) std::stoi(tokens[8].c_str());
    tokens[9].toCharArray(lbj->speed, sizeof lbj->speed);
    tokens[10].toCharArray(lbj->position, sizeof lbj->position);
    tokens[11].toCharArray(lbj->time, sizeof lbj->time);
    lbj->info2_hex = tokens[12];
    lbj->loco_type = tokens[13];

    // sprintf(buffer, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,", lbj.lbj_class, lbj.loco, lbj.route, lbj.route_utf8,
    //         lbj.pos_lon_deg, lbj.pos_lon_min, lbj.pos_lat_deg, lbj.pos_lat_min, lbj.pos_lon, lbj.pos_lat);
    tokens[14].toCharArray(lbj->lbj_class, sizeof lbj->lbj_class);
    tokens[15].toCharArray(lbj->loco, sizeof lbj->loco);
    tokens[16].toCharArray(lbj->route, sizeof lbj->route);
    tokens[17].toCharArray(lbj->route_utf8, sizeof lbj->route_utf8);
    tokens[18].toCharArray(lbj->pos_lon_deg, sizeof lbj->pos_lon_deg);
    tokens[19].toCharArray(lbj->pos_lon_min, sizeof lbj->pos_lon_min);
    tokens[20].toCharArray(lbj->pos_lat_deg, sizeof lbj->pos_lat_deg);
    tokens[21].toCharArray(lbj->pos_lat_min, sizeof lbj->pos_lat_min);
    tokens[22].toCharArray(lbj->pos_lon, sizeof lbj->pos_lon);
    tokens[23].toCharArray(lbj->pos_lat, sizeof lbj->pos_lat);

    // sprintf(buffer, "%.2f,%.2f,%.2f,%u", rx.rssi, rx.fer, rx.ppm, ids);
    rx->rssi = std::stof(tokens[24].c_str());
    rx->fer = std::stof(tokens[25].c_str());
    rx->ppm = std::stof(tokens[26].c_str());
    *id = std::stoul(tokens[27].c_str());
    lbj->epi = tokens[28];
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
    if (!have_pref)
        return;
    nvs_stats_t stats;
    nvs_get_stats(partition_name, &stats);
    Serial.printf("[NVS] %s stats: UsedEntries = %d, FreeEntries = %d, AllEntries = %d\n",
                  partition_name, stats.used_entries, stats.free_entries, stats.total_entries);
    nvs_handle_t handle;
    nvs_open_from_partition(partition_name, ns_name, NVS_READONLY, &handle);
    size_t used_entries;
    nvs_get_used_entry_count(handle, &used_entries);
    nvs_close(handle);
    Serial.printf("[NVS] %d entries in namespace %s\n", used_entries, ns_name);
    Serial.printf("[NVS] Current line %d, id %d\n", lines, ids);
}

uint32_t aPreferences::getID() {
    return ids;
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

