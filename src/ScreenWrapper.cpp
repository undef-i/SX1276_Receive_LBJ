//
// Created by FLN1021 on 2024/2/10.
//

#include "ScreenWrapper.h"

bool ScreenWrapper::setDisplay(U8G2_SSD1306_128X64_NONAME_F_HW_I2C *display_ptr) {
    this->display = display_ptr;
    if (!this->display)
        return false;

    return true;
}

void ScreenWrapper::updateInfo() {
    if (!display)
        return;

    char buffer[32];
    // update top
    if (update_top) {
        display->setDrawColor(0);
        display->setFont(u8g2_font_squeezed_b7_tr);
        display->drawBox(0, 0, 97, 8);
        display->setDrawColor(1);
        if (!getLocalTime(&time_info, 0))
            display->drawStr(0, 7, "NO SNTP");
        else {
            sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
                    time_info.tm_hour, time_info.tm_min);
            display->drawStr(0, 7, buffer);
        }
#ifdef HAS_RTC
        sprintf(buffer, "%dC", (int) rtc.getTemperature());
        display->drawStr(80, 7, buffer);
#endif

        // update bottom
        display->setDrawColor(0);
        display->drawBox(0, 56, 128, 8);
        display->setDrawColor(1);
        if (!no_wifi) {
            String ipa = WiFi.localIP().toString();
            display->drawStr(0, 64, ipa.c_str());
        } else
            display->drawStr(0, 64, "WIFI OFF");
    } else {
        display->setDrawColor(0);
        display->drawBox(73, 56, 56, 8);
        display->setDrawColor(1);
    }
    sprintf(buffer, "%.1f", getBias(actual_frequency));
    display->drawStr(73, 64, buffer);
    if (sd1.status() && WiFiClass::status() == WL_CONNECTED)
        display->drawStr(89, 64, "D");
    else if (sd1.status())
        display->drawStr(89, 64, "L");
    else if (WiFiClass::status() == WL_CONNECTED)
        display->drawStr(89, 64, "N");
    sprintf(buffer, "%2d", ets_get_cpu_frequency() / 10);
    display->drawStr(96, 64, buffer);
    voltage = battery.readVoltage() * 2;
    sprintf(buffer, "%1.2f", voltage); // todo: Implement average voltage reading.
    if (voltage < 3.15 && !low_volt_warned) {
        Serial.printf("Warning! Low Voltage detected, %1.2fV\n", voltage);
        sd1.append("低压警告，电池电压%1.2fV\n", voltage);
        low_volt_warned = true;
    }
    display->drawStr(108, 64, buffer);
    display->sendBuffer();
}

void ScreenWrapper::showInitComp() {
    if (!display)
        return;
    display->clearBuffer();
    display->setFont(u8g2_font_squeezed_b7_tr);
    // bottom (0,56,128,8)
    String ipa = WiFi.localIP().toString();
    display->drawStr(0, 64, ipa.c_str());
    if (have_sd && WiFiClass::status() == WL_CONNECTED)
        display->drawStr(89, 64, "D");
    else if (have_sd)
        display->drawStr(89, 64, "L");
    else if (WiFiClass::status() == WL_CONNECTED)
        display->drawStr(89, 64, "N");
    char buffer[32];
    sprintf(buffer, "%2d", ets_get_cpu_frequency() / 10);
    display->drawStr(96, 64, buffer);
    sprintf(buffer, "%1.2f", battery.readVoltage() * 2);
    display->drawStr(108, 64, buffer);
    // top (0,0,128,8)
    if (!getLocalTime(&time_info, 0))
        display->drawStr(0, 7, "NO SNTP");
    else {
        sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
                time_info.tm_hour, time_info.tm_min);
        display->drawStr(0, 7, buffer);
    }
    display->sendBuffer();
}

void ScreenWrapper::pword(const char *msg, int xloc, int yloc) {
    int dspW = display->getDisplayWidth();
    int strW = 0;
    char glyph[2];
    glyph[1] = 0;
    for (const char *ptr = msg; *ptr; *ptr++) {
        glyph[0] = *ptr;
        strW += display->getStrWidth(glyph);
        ++strW;
        if (xloc + strW > dspW) {
            int sxloc = xloc;
            while (msg < ptr) {
                glyph[0] = *msg++;
                xloc += display->drawStr(xloc, yloc, glyph);
            }
            strW -= xloc - sxloc;
            yloc += display->getMaxCharHeight();
            xloc = 0;
        }
    }
    while (*msg) {
        glyph[0] = *msg++;
        xloc += display->drawStr(xloc, yloc, glyph);
    }
}

void ScreenWrapper::showSTR(const String &str) {
    if (!display)
        return;
    display->setDrawColor(0);
    display->drawBox(0, 8, 128, 48);
    display->setDrawColor(1);
    // display->setFont(u8g2_font_wqy12_t_gb2312a);
    display->setFont(u8g2_font_squeezed_b7_tr);
    pword(str.c_str(), 0, 19);
    display->sendBuffer();
}

void ScreenWrapper::showLBJ0(const struct lbj_data &l, const struct rx_info &r) {
    if (!display)
        return;
    // box y 9->55
    char buffer[128];
    display->setDrawColor(0);
    display->drawBox(0, 8, 128, 48);
    display->setDrawColor(1);
    display->setFont(u8g2_font_wqy15_t_custom);
    if (l.direction == FUNCTION_UP) {
        sprintf(buffer, "车  次 %s 上行", l.train);
    } else if (l.direction == FUNCTION_DOWN)
        sprintf(buffer, "车  次 %s 下行", l.train);
    else
        sprintf(buffer, "车  次 %s %d", l.train, l.direction);
    display->drawUTF8(0, 21, buffer);
    sprintf(buffer, "速  度  %s KM/H", l.speed);
    display->drawUTF8(0, 37, buffer);
    sprintf(buffer, "公里标 %s KM", l.position);
    display->drawUTF8(0, 53, buffer);
    // draw RSSI
    display->setDrawColor(0);
    display->drawBox(98, 0, 30, 8);
    display->setDrawColor(1);
    display->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", r.rssi);
    display->drawStr(99, 7, buffer);
    display->sendBuffer();
}

void ScreenWrapper::showLBJ1(const struct lbj_data &l, const struct rx_info &r) {
    if (!display)
        return;
    char buffer[128];
    display->setDrawColor(0);
    display->drawBox(0, 8, 128, 48);
    display->setDrawColor(1);
    display->setFont(u8g2_font_wqy12_t_gb2312a);
    // line 1
    sprintf(buffer, "车:%s%s", l.lbj_class, l.train);
    display->drawUTF8(0, 19, buffer);
    sprintf(buffer, "速:%sKM/H", l.speed);
    display->drawUTF8(68, 19, buffer);
    // line 2
    sprintf(buffer, "线:%s", l.route_utf8);
    display->drawUTF8(0, 31, buffer);
    display->drawBox(67, 21, 13, 12);
    display->setDrawColor(0);
    if (l.direction == FUNCTION_UP)
        display->drawUTF8(68, 31, "上");
    else if (l.direction == FUNCTION_DOWN)
        display->drawUTF8(68, 31, "下");
    else {
        sprintf(buffer, "%d", l.direction);
        display->drawStr(71, 31, buffer);
    }
    display->setDrawColor(1);
    sprintf(buffer, "%sK", l.position);
    display->drawUTF8(86, 31, buffer);
    // line 3
    sprintf(buffer, "号:%s", l.loco);
    display->drawUTF8(0, 43, buffer);
    if (l.loco_type.length())
        display->drawUTF8(72, 43, l.loco_type.c_str());
    // line 4
    String pos;
    if (l.pos_lat_deg[1] && l.pos_lat_min[1]) {
        sprintf(buffer, "%s°%s'", l.pos_lat_deg, l.pos_lat_min);
        pos += String(buffer);
    } else {
        sprintf(buffer, "%s ", l.pos_lat);
        pos += String(buffer);
    }
    if (l.pos_lon_deg[1] && l.pos_lon_min[1]) {
        sprintf(buffer, "%s°%s'", l.pos_lon_deg, l.pos_lon_min);
        pos += String(buffer);
    } else {
        sprintf(buffer, "%s ", l.pos_lon);
        pos += String(buffer);
    }
//    sprintf(buffer,"%s°%s'%s°%s'",l.pos_lat_deg,l.pos_lat_min,l.pos_lon_deg,l.pos_lon_min);
    display->drawUTF8(0, 54, pos.c_str());
    // draw RSSI
    display->setDrawColor(0);
    display->drawBox(98, 0, 30, 8);
    display->setDrawColor(1);
    display->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", r.rssi);
    display->drawStr(99, 7, buffer);
    display->sendBuffer();
}

void ScreenWrapper::showLBJ2(const struct lbj_data &l, const struct rx_info &r) {
    if (!display)
        return;
    char buffer[128];
    display->setDrawColor(0);
    display->drawBox(0, 8, 128, 48);
    display->setDrawColor(1);
    display->setFont(u8g2_font_wqy15_t_custom);
    sprintf(buffer, "当前时间 %s ", l.time);
    display->drawUTF8(0, 21, buffer);
    // draw RSSI
    display->setDrawColor(0);
    display->drawBox(98, 0, 30, 8);
    display->setDrawColor(1);
    display->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", r.rssi);
    display->drawStr(99, 7, buffer);
    display->sendBuffer();
}

void ScreenWrapper::showLBJ(const lbj_data &l, const rx_info &r) {
    if (!display)
        return;
    if (l.type == 0)
        showLBJ0(l, r);
    else if (l.type == 1) {
        showLBJ1(l, r);
    } else if (l.type == 2) {
        showLBJ2(l, r);
    }
}

void ScreenWrapper::showLBJ(const struct lbj_data &l, const struct rx_info &r, const String &time_str, uint16_t lines) {
    if (!display)
        return;
    if (update_top)
        update_top = false;

    // show msg rx time
    display->setDrawColor(0);
    display->setFont(u8g2_font_squeezed_b7_tr);
    display->drawBox(0, 0, 97, 8);
    display->setDrawColor(1);
    if (std::stoi(time_str.substring(0, 4).c_str()) < 2016)
        display->drawStr(0, 7, "NO SNTP");
    else {
        display->drawStr(0, 7, (time_str.substring(0, 16)).c_str());
    }

    // show msg lines
    display->setDrawColor(0);
    display->drawBox(0, 56, 72, 8);
    display->setDrawColor(1);
    char buffer[32];
    sprintf(buffer, "%04d", lines);
    display->drawStr(0, 64, buffer);
    // if (!no_wifi) {
    //     String ipa = WiFi.localIP().toString();
    //     display->drawStr(0, 64, ipa.c_str());
    // } else
    //     display->drawStr(0, 64, "WIFI OFF");
    display->sendBuffer();

    if (l.type == 0)
        showLBJ0(l, r);
    else if (l.type == 1) {
        showLBJ1(l, r);
    } else if (l.type == 2) {
        showLBJ2(l, r);
    }
}

void ScreenWrapper::resumeUpdate() {
    update_top = true;
}

