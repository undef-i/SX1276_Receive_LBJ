//
// Created by FLN1021 on 2024/2/7.
//

#include "aFFS.h"

bool aFFS::begin() {
    if (!spiffs.begin()) {
        serial.println("[SPIFFS] Failed to Mount SPIFFS, formatting...");
        // sd.append("[SPIFFS] 挂载SPIFFS失败，正在格式化SPIFFS...\n");
        SPIFFS.format();
        if (!spiffs.begin()) {
            serial.println("[SPIFFS] Failed to mount SPIFFS");
            // sd.append("[SPIFFS] 挂载SPIFFS失败\n");
            return false;
        }
    }
    serial.printf("[SPIFFS] Used %d Bytes / %d Bytes total\n", spiffs.usedBytes(), spiffs.totalBytes());
    // sd.append("[SPIFFS] 挂载SPIFFS成功，可用空间 %d B/ %d B\n", spiffs.usedBytes(), spiffs.totalBytes());
    have_spiffs = true;
    return true;
}

bool aFFS::listDir(const char *path) {
    File root = spiffs.open(path);
    if (!root) {
        serial.printf("[SPIFFS] Open %s failed\n", path);
        return false;
    }
    if (!root.isDirectory()) {
        Serial.printf("[SPIFFS] %s is not a directory\n", path);
        return false;
    }

    File file = root.openNextFile();
    if (!file) {
        serial.printf("[SPIFFS] Empty directory: %s\n", path);
    } else {
        serial.printf("[SPIFFS] File in directory %s:\n", path);
    }
    while (file) {
        if (file.isDirectory()) {
            serial.printf("[SPIFFS] DIR  %s\n", file.name());
        } else {
            serial.printf("[SPIFFS] FILE %s, %d B\n", file.name(), file.size());
        }
        file = root.openNextFile();
    }
    return true;
}

aFFS::aFFS() :
        serial(Serial), spiffs(), have_spiffs(false), cache_path{}, cache_file{}, lines(0), ret_line(0), lines_pos{} {}

void aFFS::setIO(SPIFFSFS &flash, HardwareSerial &hs) {
    this->serial = hs;
    this->spiffs = flash;
}

bool aFFS::readFile(const char *path) {
    if (!have_spiffs)
        return false;
    File file = spiffs.open(path);
    if (!file || file.isDirectory()) {
        serial.println("[SPIFFS] Failed to open file for reading");
        return false;
    }

    serial.println("[SPIFFS] Read from file:");
    while (file.available()) {
        serial.write(file.read());
    }
    file.close();
    return true;
}

void aFFS::usePath(const char *path, const char *name) {
    if (!have_spiffs)
        return;
    File file = spiffs.open((String(path) + String(name)), "r", true);
    if (!file) {
        serial.printf("[SPIFFS] Failed to open %s\n", (String(path) + String(name)).c_str());
        have_spiffs = false;
        return;
    }
    serial.printf("[SPIFFS] Using file %s, size %d B.\n", file.name(), file.size());
    // cache_file = file;
    cache_path = String(path) + String(name);
    while (file.available()) {
        lines_pos[lines] = file.position();
        file.readStringUntil('\n');
        lines++;
    }
    serial.printf("[SPIFFS] %d lines in %s\n", lines, file.name());
    file.close();
    // cache_file = spiffs.open(cache_path, "a");
}

void aFFS::append(lbj_data lbj, rx_info rx, float volt, float temp) {
    if (!have_spiffs)
        return;
    cache_file = spiffs.open(cache_path, "a");
    if (!cache_file) {
        have_spiffs = false;
        return;
    }
    /* Standard format of cache:
     * 条目数,电压,系统时间,温度,日期,时间,type,train,direction,speed,position,time,info2_hex,loco_type,lbj_class,loco,route,
     * route_utf8,pos_lon_deg,pos_lon_min,pos_lat_deg,pos_lat_min,pos_lon,pos_lat,rssi,fer,ppm
     */
    if (lines > FLASH_MAX_LINES) {
        // todo: add countermeasures (cycling).
    }
    String line;
    char buffer[256];
    struct tm now{};
    getLocalTime(&now, 1);
    sprintf(buffer, "%04d,%1.2f,%llu,", lines, volt, esp_timer_get_time());
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
    // rssi,fer,ppm
    sprintf(buffer, "%.2f,%.2f,%.2f", rx.rssi, rx.fer, rx.ppm);
    line += buffer;

    lines_pos[lines] = cache_file.position();
    cache_file.println(line);
    lines++;
    cache_file.close();
}

bool aFFS::clearCache() {
    if (!have_spiffs)
        return false;
    // cache_file.close();
    spiffs.remove(cache_path);
    cache_file = spiffs.open(cache_path, FILE_APPEND, true);
    if (cache_file.size() != 0)
        return false;
    lines = 0;
    cache_file.close();
    return true;
}

bool aFFS::retrieve(lbj_data *lbj, rx_info *rx, bool last) {
    if (!have_spiffs)
        return false;
    if (ret_line == 0)
        ret_line = lines;
    if (last && ret_line != 0)
        ret_line--;
    else if (!last && ret_line < lines - 1)
        ret_line++;
    else if (!last && ret_line == lines - 1) {}
    else
        return false;
    serial.printf("[D] ret_line = %d\n", ret_line);
    String line;
    if (!retrieveLine(ret_line, &line))
        return false;
    String tokens[27];
    for (size_t i = 0, c = 0; i < line.length(); i++) {
        if (line[i] == ',') {
            c++;
            continue;
        }
        tokens[c] += line[i];
    }
    // serial.printf("[D]%d,%s,%d\n",lbj->type,lbj->train,lbj->direction);
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

    // sprintf(buffer, "%.2f,%.2f,%.2f", rx.rssi, rx.fer, rx.ppm);
    rx->rssi = std::stof(tokens[24].c_str());
    rx->fer = std::stof(tokens[25].c_str());
    rx->ppm = std::stof(tokens[26].c_str());
    return true;
}

bool aFFS::retrieveLine(uint32_t line, String *line_str) {
    if (!have_spiffs)
        return false;
    cache_file = spiffs.open(cache_path, FILE_READ);
    if (!cache_file.available())
        return false;
    // size_t pos = cache_file.size();
    // char buf[5];
    // sprintf(buf,"%04d",line);
    // while (pos) {
    //     pos--;
    //     uint64_t time = millis64();
    //     cache_file.seek(pos);
    //     if (cache_file.read() == '\n') {
    //         serial.printf("time %llu ms\n", millis64() - time);
    //         serial.println("[D] find LF");
    //         String str = cache_file.readStringUntil(',');
    //         serial.printf("[D] str %s, tgt %s\n",str.c_str(),buf);
    //         if (str == String(buf)) {
    //             pos++;
    //             break;
    //         }
    //     }
    // }
    // cache_file.seek(pos);
    cache_file.seek(lines_pos[line]);
    *line_str = cache_file.readStringUntil('\n');
    serial.printf("[D] %s \n", line_str->c_str());
    return true;
}

uint32_t aFFS::getCurrentLine() const {
    return lines;
}

void aFFS::toLatest() {
    ret_line = lines - 1;
}
