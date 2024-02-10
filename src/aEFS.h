//
// Created by FLN1021 on 2024/2/9.
//

#ifndef PAGER_RECEIVE_AEFS_H
#define PAGER_RECEIVE_AEFS_H

#include "networks.hpp"
#include "Effortless_SPIFFS.h"

const int CACHE_MAX_LINES = 2500;

class aEFS {
public:
    aEFS();
    bool begin();
    bool append(lbj_data lbj, rx_info rx, float volt, float temp);
    bool retrieve(lbj_data *lbj, rx_info *rx, int8_t bias);
    bool clearCache();
    void toLatest();
private:
    eSPIFFS fs;
    SPIFFSFS spiffs;
    bool have_spiffs;
    bool overflow;
    uint16_t lines;
    int32_t ret_lines; // defaults -1
    uint32_t ids;

};


#endif //PAGER_RECEIVE_AEFS_H
