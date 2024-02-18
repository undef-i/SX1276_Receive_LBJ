//
// Created by FLN1021 on 2024/2/10.
//

#ifndef PAGER_RECEIVE_SCREENWRAPPER_H
#define PAGER_RECEIVE_SCREENWRAPPER_H

#include <cstdint>
#include "networks.hpp"
#include "customfont.h"
#include "aPreferences.h"

#ifdef HAS_DISPLAY

enum top_sectors{
    TOP_SECTOR_TIME,
    TOP_SECTOR_TEMPERATURE,
    TOP_SECTOR_RSSI,
    TOP_SECTOR_ALL
};

enum bottom_sectors{
    BOTTOM_SECTOR_IP,
    BOTTOM_SECTOR_PPM,
    BOTTOM_SECTOR_IND,
    BOTTOM_SECTOR_CPU,
    BOTTOM_SECTOR_BAT,
    BOTTOM_SECTOR_ALL
};

class ScreenWrapper {
public:
    bool setDisplay(DISPLAY_MODEL *display_ptr);

    void setFlash(aPreferences *flash_cls);

    void updateInfo();

    void showInitComp();

    void showListening();

    void showSTR(const String &str);

    void showLBJ(const struct lbj_data &l, const struct rx_info &r);

    void showLBJ0(const struct lbj_data &l, const struct rx_info &r);

    void showLBJ1(const struct lbj_data &l, const struct rx_info &r);

    void showLBJ2(const struct lbj_data &l, const struct rx_info &r);

    void showLBJ(const struct lbj_data &l, const struct rx_info &r, const String &time_str, uint16_t lines,
                 uint32_t id, float temp);

    void showSelectedLBJ(int8_t bias);

    void showSelectedLBJ(aPreferences *flash_cls , int8_t bias);

    void showInfo(int8_t page = 1);

    void resumeUpdate();

    void clearTop(top_sectors sector, bool sendBuffer);
    void clearCenter(bool sendBuffer);
    void clearBottom(bottom_sectors sector, bool sendBuffer);
    void clearAll();

protected:
    void pwordUTF8(const String& msg, int xloc, int yloc, int xmax, int ymax);
    bool update = true;
    DISPLAY_MODEL *display = nullptr;
    aPreferences *flash;

private:
    void pword(const char *msg, int xloc, int yloc);
    bool low_volt_warned = false;
    bool update_top = true;
};

#endif
#endif //PAGER_RECEIVE_SCREENWRAPPER_H
