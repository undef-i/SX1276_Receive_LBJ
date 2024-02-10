//
// Created by FLN1021 on 2024/2/10.
//

#ifndef PAGER_RECEIVE_SCREENWRAPPER_H
#define PAGER_RECEIVE_SCREENWRAPPER_H

#include "networks.hpp"
#include "customfont.h"

#ifdef HAS_DISPLAY

class ScreenWrapper {
public:
    bool setDisplay(DISPLAY_MODEL *display_ptr);

    void updateInfo();

    void showInitComp();

    void showSTR(const String &str);

    void showLBJ(const struct lbj_data &l, const struct rx_info &r);

    void showLBJ0(const struct lbj_data &l, const struct rx_info &r);

    void showLBJ1(const struct lbj_data &l, const struct rx_info &r);

    void showLBJ2(const struct lbj_data &l, const struct rx_info &r);

    void showLBJ(const struct lbj_data &l, const struct rx_info &r, const String &time_str, uint16_t lines);

    void resumeUpdate();

private:
    void pword(const char *msg, int xloc, int yloc);

    DISPLAY_MODEL *display = nullptr;
    bool low_volt_warned = false;
    bool update_top = true;
};

#endif
#endif //PAGER_RECEIVE_SCREENWRAPPER_H
