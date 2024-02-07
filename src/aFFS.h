//
// Created by FLN1021 on 2024/2/7.
//

#ifndef PAGER_RECEIVE_AFFS_H
#define PAGER_RECEIVE_AFFS_H

#include <SPIFFS.h>
#include "networks.hpp"

const int FLASH_MAX_LINES = 2500;

class aFFS {
public:

    aFFS();

    void setIO(SPIFFSFS &flash, HardwareSerial &hs);

    bool begin();

    bool listDir(const char *path);

    bool readFile(const char *path);

    void usePath(const char *path, const char *name);

    void append(lbj_data lbj, rx_info rx, float volt, float temp);

    bool retrieve(lbj_data *lbj, rx_info *rx, bool last);

    bool retrieveLine(uint32_t line, String *line_str);

    bool clearCache();

    uint32_t getCurrentLine() const;

    void toLatest();


private:
    bool have_spiffs;
    HardwareSerial serial;
    fs::SPIFFSFS spiffs;
    String cache_path;
    File cache_file;
    uint32_t lines;
    uint32_t ret_line;
    size_t lines_pos[FLASH_MAX_LINES + 1];
};


#endif //PAGER_RECEIVE_AFFS_H
