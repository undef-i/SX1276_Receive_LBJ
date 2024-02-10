//
// Created by FLN1021 on 2024/2/9.
//

#include "aEFS.h"

bool aEFS::begin() {
    if(!fs.checkFlashConfig())
        return false;
    Serial.printf("[D] SPIFFS %d / %d Bytes\n",spiffs.usedBytes(),spiffs.totalBytes());
    have_spiffs = true;
    if (spiffs.exists("/lines"))
        fs.openFromFile("/lines",lines);
    ret_lines = lines;
    char buf[8];
    sprintf(buf,"/I%04d",lines + 1);
    if (spiffs.exists(buf))
        overflow = true;
    return true;
}

bool aEFS::append(lbj_data lbj, rx_info rx, float volt, float temp) {
    if (!have_spiffs)
        return false;
    /* Standard format of cache:
     * 条目数,电压,系统时间,温度,日期,时间,type,train,direction,speed,position,time,info2_hex,loco_type,lbj_class,loco,route,
     * route_utf8,pos_lon_deg,pos_lon_min,pos_lat_deg,pos_lat_min,pos_lon,pos_lat,rssi,fer,ppm
     */
    String line;
    char buffer[256];
    struct tm now{};
    getLocalTime(&now, 1);

    if (lines > CACHE_MAX_LINES) {
        // todo: add countermeasures (cycling).
        lines = 0;
    }
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

    sprintf(buffer, "/I%04d", lines);
    fs.saveToFile(buffer,line);
    lines++;
    fs.saveToFile("/lines",lines);
    // ids++;
    return true;
}

bool aEFS::retrieve(lbj_data *lbj, rx_info *rx, int8_t bias) {
    if (!have_spiffs)
        return false;
    ret_lines += bias;
    if (overflow) {
        if (ret_lines < 0)
            ret_lines = CACHE_MAX_LINES;
        if (ret_lines > CACHE_MAX_LINES)
            ret_lines = 0;
    } else {
        if (ret_lines < 0)
            ret_lines = lines - 1;
        if (ret_lines == lines)
            ret_lines = 0;
    }

    char buf[8];
    sprintf(buf, "/I%04d", ret_lines);
    if (!spiffs.exists(buf))
        return false;
    Serial.printf("[D] ret_line = %d\n", ret_lines);
    String line;
    fs.openFromFile(buf, line);
    Serial.printf("[D] %s \n", line.c_str());
    // Tokenize
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

bool aEFS::clearCache() {
    if (!have_spiffs)
        return false;
    char buf[8];
    for (int i = 0; i < lines; ++i) {
        sprintf(buf, "/I%04d",i);
        if (spiffs.exists(buf))
            spiffs.remove(buf);
    }
    lines = 0;
    fs.saveToFile("/lines",lines);
    ret_lines = 0;
    return true;
}

void aEFS::toLatest() {
    ret_lines = lines - 1;
}
