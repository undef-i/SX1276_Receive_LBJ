//
// Created by FLN1021 on 2024/2/10.
//

#include "ScreenWrapper.h"

bool ScreenWrapper::setDisplay(DISPLAY_MODEL *display_ptr) {
    this->display = display_ptr;
    if (!this->display)
        return false;

    return true;
}

void ScreenWrapper::updateInfo() {
    if (!display || !update || !enabled)
        return;

    char buffer[32];
    // update top - battery percentage
    if (update_top) {
        display->setDrawColor(0);
        display->setFont(u8g2_font_squeezed_b7_tr);
        display->drawBox(0, 0, 98, 8);
        display->setDrawColor(1);
        voltage = battery.readVoltage() * 2;
        int batteryPercentage = ((voltage - 3.15) / (4.2 - 3.15)) * 100;
        if (batteryPercentage > 100) batteryPercentage = 100;
        if (batteryPercentage < 0) batteryPercentage = 0;
        sprintf(buffer, "%d%%", batteryPercentage);
        display->drawStr(0, 7, buffer);
#ifdef HAS_RTC
        if (have_rtc) {
            sprintf(buffer, "%dC", (int) rtc.getTemperature());
            display->drawStr(80, 7, buffer);
        }
#endif

        // update bottom
        display->setDrawColor(0);
        display->drawBox(0, 56, 128, 8);
        display->setDrawColor(1);
        display->drawStr(0, 64, BLE_DEVICE_NAME);
    } else {
        display->setDrawColor(0);
        display->drawBox(73, 56, 56, 8);
        display->setDrawColor(1);
    }
    sprintf(buffer, "%.1f", getBias(actual_frequency));  // PPM偏差值
    display->drawStr(73, 64, buffer);
    sprintf(buffer, "%dMHz", ets_get_cpu_frequency());  // CPU频率完整显示
    int strWidth = display->getStrWidth(buffer);
    display->drawStr(128 - strWidth, 64, buffer);  // 右对齐显示
    display->sendBuffer();
}

void ScreenWrapper::showInitComp() {
    if (!display)
        return;
    display->clearBuffer();
    display->setFont(u8g2_font_squeezed_b7_tr);
    // bottom (0,56,128,8)
    display->drawStr(0, 64, BLE_DEVICE_NAME);
    char buffer[32];
    sprintf(buffer, "%2u", ets_get_cpu_frequency() / 10);
    display->drawStr(96, 64, buffer);
    sprintf(buffer, "%1.2f", battery.readVoltage() * 2);
    display->drawStr(108, 64, buffer);
    // top (0,0,128,8)
    sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
            time_info.tm_hour, time_info.tm_min);
    display->drawStr(0, 7, buffer);
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
    // display->setFont(FONT_12_GB2312);
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
    display->setCursor(0, 21);
    display->printf("车  次");
    display->setFont(font_15_alphanum_bold);
    display->setCursor(50, display->getCursorY());
    display->printf("%s", l.train);
    // draw epi
    // drawEpi(l.type, 0, l.epi);
    directDrawEpi(getErrorCount(0, l.epi), 50, l.train, 1, 1, 1);
    display->setFont(u8g2_font_wqy15_t_custom);
    display->setCursor(display->getCursorX() + 6, display->getCursorY());
    if (l.direction == FUNCTION_UP) {
        display->printf("上行");
    } else if (l.direction == FUNCTION_DOWN)
        display->printf("下行");
    else
        display->printf("%d", l.direction);
    // if (l.direction == FUNCTION_UP) {
    //     sprintf(buffer, "车  次 %s 上行", l.train);
    // } else if (l.direction == FUNCTION_DOWN)
    //     sprintf(buffer, "车  次 %s 下行", l.train);
    // else
    //     sprintf(buffer, "车  次 %s %d", l.train, l.direction);
    // display->drawUTF8(0, 21, buffer);
    // sprintf(buffer, "速  度  %s KM/H", l.speed);
    display->setCursor(0, 37);
    display->printf("速  度");
    u8g2->setCursor(50, u8g2->getCursorY());
    display->setFont(font_15_alphanum_bold);
    display->printf(" %s ", l.speed);
    // drawEpi(l.type, 1, l.epi);
    directDrawEpi(getErrorCount(1, l.epi), 50, l.speed, 1, 1, 1);
    u8g2->setCursor(u8g2->getCursorX() + 7, u8g2->getCursorY());
    display->setFont(font_15_alphanum);
    display->printf("KM/H");
    display->setFont(u8g2_font_wqy15_t_custom);
    // sprintf(buffer, "公里标 %s KM", l.position);
    display->setCursor(0, 53);
    display->printf("公里标");
    u8g2->setCursor(50, u8g2->getCursorY());
    display->setFont(font_15_alphanum_bold);
    display->printf("%s", l.position);
    // drawEpi(l.type, 2, l.epi);
    directDrawEpi(getErrorCount(2, l.epi), 50, l.position, 1, 1, 1);
    display->printf(" ");
    display->setCursor(display->getCursorX() + 4, display->getCursorY());
    display->setFont(font_15_alphanum);
    display->printf("KM");
    // display->drawUTF8(0, 53, buffer);
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
    display->setFont(FONT_12_GB2312);
    // line 1
    display->setCursor(0, 19);
    // sprintf(buffer, "车:%s%s", l.lbj_class, l.train);
    display->printf("车:");
    display->setCursor(display->getCursorX() + 2, display->getCursorY());
    display->setFont(font_12_alphanum);
    // for (int i = 0, c = 0; i < 8; i++) {
    //     if (i == 7) {
    //         buffer[c] = 0;
    //         break;
    //     }
    //     if (i < 2) {
    //         if (l.lbj_class[i] == ' ')
    //             continue;
    //         buffer[c] = l.lbj_class[i];
    //     } else {
    //         if (l.train[i - 2] == ' ')
    //             continue;
    //         buffer[c] = l.train[i - 2];
    //     }
    //     ++c;
    // }
    // display->printf("%s", buffer);
    for (int i = 0, c = 0; i < 6; i++) {
        if (i == 5) {
            buffer[c] = 0;
            break;
        }
        if (l.train[i] == ' ')
            continue;
        buffer[c] = l.train[i];
        ++c;
    }
    // 处理车次类型，去除前面的空格
    char class_buffer[3] = {0};
    for (int i = 0, c = 0; i < 2; i++) {
        if (l.lbj_class[i] == ' ')
            continue;
        class_buffer[c] = l.lbj_class[i];
        ++c;
    }
    
    uint16_t cx_prev = display->getCursorX();
    display->printf("%s", class_buffer);
    directDrawEpi(getErrorCount(3, l.epi), cx_prev, l.lbj_class, 0, 1);
    // drawEpi(l.type, 3, l.epi, cx_prev);
    cx_prev = display->getCursorX();
    display->printf("%s", buffer);
    directDrawEpi(getErrorCount(0, l.epi), cx_prev, buffer, 0, 1);
    // drawEpi(l.type, 0, l.epi, cx_prev);
    // display->printf("%s%s", l.lbj_class, l.train);
    display->setFont(FONT_12_GB2312);
    // sprintf(buffer, "速:%sKM/H", l.speed);
    display->setCursor(68, 19);
    display->printf("速:");
    display->setCursor(display->getCursorX() + 2, display->getCursorY());
    display->setFont(font_12_alphanum);
    cx_prev = display->getCursorX();
    display->printf("%s", l.speed);
    // drawEpi(l.type, 1, l.epi, cx_prev);
    directDrawEpi(getErrorCount(1, l.epi), cx_prev, l.speed, 0, 1);
    display->setCursor(display->getCursorX(), display->getCursorY());
    display->printf("KM/H");
    display->setFont(FONT_12_GB2312);
    // display->drawUTF8(68, 19, buffer);
    // line 2
    // sprintf(buffer, "线:%s", l.route_utf8);
    display->setCursor(0, 31);
    display->printf("线:");
    display->setCursor(display->getCursorX() + 2, display->getCursorY());
    // cx_prev = display->getCursorX();
    if (String(l.route_utf8) == "********")
        display->printf("%s", l.route_utf8);
    else {
        String route_str = String(l.route_utf8);
        bool short_ch = false;
        for (int i = 0, j = 5; i < route_str.length();) {
            int8_t char_len = getU8CharLen(route_str[i]);
            String route_char = route_str.substring(i, i + char_len);
            if (!short_ch)
                cx_prev = display->getCursorX();
            display->print(route_char);
            // Serial.println(route_char);
            if (char_len < 2 && !short_ch) {
                short_ch = true;
                i += char_len;
                continue;
            }
            if (short_ch) {
                short_ch = false;
                route_char = route_str.substring(i - 1, i) + route_char;
            }
            if (j < 8) {
                if (getErrorCount(j, l.epi) != -1 || getErrorCount(j + 1, l.epi) != -1) {
                    int8_t error = max(getErrorCount(j, l.epi), getErrorCount(j + 1, l.epi));
                    directDrawEpi(error, cx_prev, route_char, 1);
                }
            } else {
                if (getErrorCount(j, l.epi) != -1) {
                    int8_t error = getErrorCount(j, l.epi);
                    directDrawEpi(error, cx_prev, route_char, 1);
                }
            }
            j++;
            i += char_len;
        }
        // String route_char = String(l.route_utf8).substring(0,3);
        // cx_prev = display->getCursorX();
        // display->print(route_char);
        // Serial.println(route_char);
        // if (getErrorCount(5,l.epi)!=-1 || getErrorCount(6,l.epi)!=-1) {
        //     int8_t error = max(getErrorCount(5,l.epi), getErrorCount(6,l.epi));
        //     if (error == 1) {
        //         display->drawHLine(cx_prev, display->getCursorY()+1, display->getCursorX()-cx_prev);
        //     } else if (error == 2) {
        //         uint16_t cy_prev = display->getCursorY();
        //         display->drawBox(cx_prev, display->getCursorY()-display->getMaxCharHeight()+3,display->getCursorX()-cx_prev,display->getMaxCharHeight()-1);
        //         display->setDrawColor(0);
        //         display->setCursor(cx_prev,cy_prev);
        //         display->print(route_char);
        //         display->setDrawColor(1);
        //     }
        // }
        // // drawEpi(l.type, 5, l.epi, cx_prev);
        // // uint16_t cx_prev2 = display->getCursorX();
        // cx_prev = display->getCursorX();
        // route_char = String(l.route_utf8).substring(3,6);
        // display->print(route_char);
        // Serial.println(route_char);
        // if (getErrorCount(6,l.epi)!=-1 || getErrorCount(7,l.epi)!=-1) {
        //     int8_t error = max(getErrorCount(6,l.epi), getErrorCount(7,l.epi));
        //     if (error == 1) {
        //         display->drawHLine(cx_prev, display->getCursorY()+1, display->getCursorX()-cx_prev);
        //     } else if (error == 2) {
        //         uint16_t cy_prev = display->getCursorY();
        //         display->drawBox(cx_prev, display->getCursorY()-display->getMaxCharHeight()+3,display->getCursorX()-cx_prev,display->getMaxCharHeight()-1);
        //         display->setCursor(cx_prev,cy_prev);
        //         display->setDrawColor(0);
        //         display->print(route_char);
        //         display->setDrawColor(1);
        //     }
        // }
        // // drawEpi(l.type, 6, l.epi, cx_prev);
        // cx_prev = display->getCursorX();
        // display->printf("%s", String(l.route_utf8).substring(6, 9).c_str());
        // Serial.println(route_char);
        // if (getErrorCount(7,l.epi)!=-1 || getErrorCount(8,l.epi)!=-1) {
        //     int8_t error = max(getErrorCount(7,l.epi), getErrorCount(8,l.epi));
        //     if (error == 1) {
        //         display->drawHLine(cx_prev, display->getCursorY()+1, display->getCursorX()-cx_prev);
        //     } else if (error == 2) {
        //         uint16_t cy_prev = display->getCursorY();
        //         display->drawBox(cx_prev, display->getCursorY()-display->getMaxCharHeight()+3,display->getCursorX()-cx_prev,display->getMaxCharHeight()-1);
        //         display->setDrawColor(0);
        //         display->setCursor(cx_prev,cy_prev);
        //         display->print(route_char);
        //         // Serial.println(route_char);
        //         display->setDrawColor(1);
        //     }
        // }
        // // drawEpi(l.type, 7, l.epi, cx_prev2);
        // cx_prev = display->getCursorX();
        // display->printf("%s", String(l.route_utf8).substring(9).c_str());
        // Serial.println(route_char);
        // if (getErrorCount(8,l.epi)!=-1) {
        //     if (getErrorCount(8,l.epi) == 1) {
        //         display->drawHLine(cx_prev, display->getCursorY()+1, display->getCursorX()-cx_prev);
        //     } else if (getErrorCount(8,l.epi) == 2) {
        //         uint16_t cy_prev = display->getCursorY();
        //         display->drawBox(cx_prev, display->getCursorY()-display->getMaxCharHeight()+3,display->getCursorX()-cx_prev,display->getMaxCharHeight()-1);
        //         display->setDrawColor(0);
        //         display->setCursor(cx_prev,cy_prev);
        //         display->print(route_char);
        //         // Serial.println(route_char);
        //         display->setDrawColor(1);
        //     }
        // }
        // Serial.println(l.route_utf8);
        // drawEpi(l.type, 8, l.epi, cx_prev);
        // 谢谢你，变长的UTF8，我终于绷不住了...
        // 这段非常难绷，等有机会给他整理一下吧...
        // cx_prev = display->getCursorX();
        // int8_t w = display->getMaxCharWidth();
        // display->printf("%s", l.route_utf8);
        // uint16_t cx = display->getCursorX();
        // uint16_t cy = display->getCursorY();
        // if (getErrorCount(5,l.epi)!=-1 || getErrorCount(6,l.epi)!=-1) {
        //     int8_t error = max(getErrorCount(5,l.epi), getErrorCount(6,l.epi));
        //         if (error == 1) {
        //             display->drawHLine(cx_prev,display->getCursorY()+1,w);
        //         } else if (error == 2) {
        //             display->drawFrame(cx_prev, display->getCursorY()-display->getMaxCharHeight()+2,w,display->getMaxCharHeight());
        //         }
        // }
        // cx_prev += w - 1;
        // if (getErrorCount(6,l.epi)!=-1 || getErrorCount(7,l.epi)!=-1) {
        //     int8_t error = max(getErrorCount(6,l.epi), getErrorCount(7,l.epi));
        //     if (error == 1) {
        //         display->drawHLine(cx_prev,display->getCursorY()+1,w);
        //     } else if (error == 2) {
        //         display->drawFrame(cx_prev, display->getCursorY()-display->getMaxCharHeight()+2,w,display->getMaxCharHeight());
        //     }
        // }
        // cx_prev += w - 1;
        // if (getErrorCount(7,l.epi)!=-1 || getErrorCount(8,l.epi)!=-1) {
        //     int8_t error = max(getErrorCount(7,l.epi), getErrorCount(8,l.epi));
        //     if (error == 1) {
        //         display->drawHLine(cx_prev,display->getCursorY()+1,w);
        //     } else if (error == 2) {
        //         display->drawFrame(cx_prev, display->getCursorY()-display->getMaxCharHeight()+2,w,display->getMaxCharHeight());
        //     }
        // }
        // cx_prev += w - 1;
        // if (getErrorCount(8,l.epi)!=-1) {
        //     int8_t error = getErrorCount(8,l.epi);
        //     if (error == 1) {
        //         display->drawHLine(cx_prev,display->getCursorY()+1,w);
        //     } else if (error == 2) {
        //         display->drawFrame(cx_prev, display->getCursorY()-display->getMaxCharHeight()+2,w,display->getMaxCharHeight());
        //     }
        // }
        // display->setCursor(cx,cy);
    }
    // display->drawUTF8(0, 31, buffer);
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
    // sprintf(buffer, "%sK", l.position);
    // display->drawUTF8(86, 31, buffer);
    display->setCursor(84, 31);
    display->setFont(font_12_alphanum);
    cx_prev = display->getCursorX();
    display->printf("%s", l.position);
    // drawEpi(l.type, 2, l.epi, cx_prev);
    directDrawEpi(getErrorCount(2, l.epi), cx_prev, l.position, 0, 1);
    display->setCursor(display->getCursorX(), display->getCursorY());
    display->printf("K");
    display->setFont(FONT_12_GB2312);
    // line 3
    // sprintf(buffer, "号:%s", l.loco);
    display->setCursor(0, 43);
    display->setFont(font_12_alphanum);
    if (String(l.loco) == "<NUL>") {
        display->printf("%s", l.loco);
    } else {
        cx_prev = display->getCursorX();
        display->printf("%c", l.loco[0]);
        // drawEpi(l.type, 3, l.epi, cx_prev);
        directDrawEpi(getErrorCount(3, l.epi), cx_prev, String(l.loco[0]), 0, 1);
        cx_prev = display->getCursorX();
        display->printf("%s", String(l.loco).substring(1, 6).c_str());
        // drawEpi(l.type, 4, l.epi, cx_prev);
        directDrawEpi(getErrorCount(4, l.epi), cx_prev, String(l.loco).substring(1, 6), 0, 1);
        cx_prev = display->getCursorX();
        if (l.info2_hex.length() > 14 && l.info2_hex[12] == '3') {
            if (l.info2_hex[13] == '1')
                sprintf(buffer, "%sA", String(l.loco).substring(6).c_str());
            else if (l.info2_hex[13] == '2')
                sprintf(buffer, "%sB", String(l.loco).substring(6).c_str());
            else
                sprintf(buffer, "%s", String(l.loco).substring(6).c_str());
        } else {
            sprintf(buffer, "%s", String(l.loco).substring(6).c_str());
        }
        display->printf("%s", buffer);
        // drawEpi(l.type, 5, l.epi, cx_prev);
        directDrawEpi(getErrorCount(5, l.epi), cx_prev, buffer, 0, 1);

    }
    display->setFont(FONT_12_GB2312);
    // display->drawUTF8(0, 43, buffer);
    if (l.loco_type.length()) {
        // 计算机车类型字符串宽度，实现右对齐
        int strWidth = display->getUTF8Width(l.loco_type.c_str());
        display->drawUTF8(128 - strWidth, 43, l.loco_type.c_str());
    }
    // line 4
    String pos;
    display->setCursor(0, 54);
    display->setFont(font_12_alphanum);
    if (l.pos_lat_deg[1] && l.pos_lat_min[1]) {
        sprintf(buffer, "%c", l.pos_lat_deg[0]);
        cx_prev = display->getCursorX();
        display->print(buffer);
        directDrawEpi(getErrorCount(10, l.epi), cx_prev, buffer, 0, 1);
        sprintf(buffer, "%c°%s", l.pos_lat_deg[1], String(l.pos_lat_min).substring(0, 4).c_str());
        cx_prev = display->getCursorX();
        display->print(buffer);
        directDrawEpi(getErrorCount(11, l.epi), cx_prev, buffer, 0, 1);
        sprintf(buffer, "%s'", String(l.pos_lat_min).substring(4).c_str());
        cx_prev = display->getCursorX();
        display->print(buffer);
        directDrawEpi(getErrorCount(12, l.epi), cx_prev, buffer, 0, 1);
        // sprintf(buffer, "%s°%s'", l.pos_lat_deg, l.pos_lat_min);
        // pos += String(buffer);
    } else {
        sprintf(buffer, "%s ", l.pos_lat);
        display->print(buffer);
        // pos += String(buffer);
    }
    if (l.pos_lon_deg[1] && l.pos_lon_min[1]) {
        sprintf(buffer, "%s°%s", l.pos_lon_deg, String(l.pos_lon_min).substring(0, 3).c_str());
        cx_prev = display->getCursorX();
        display->print(buffer);
        directDrawEpi(getErrorCount(9, l.epi), cx_prev, buffer, 0, 1);
        sprintf(buffer, "%s'", String(l.pos_lon_min).substring(3).c_str());
        cx_prev = display->getCursorX();
        display->print(buffer);
        directDrawEpi(getErrorCount(10, l.epi), cx_prev, buffer, 0, 1);
        // sprintf(buffer, "%s°%s'", l.pos_lon_deg, l.pos_lon_min);
        // pos += String(buffer);
    } else {
        sprintf(buffer, "%s ", l.pos_lon);
        display->print(buffer);
        // pos += String(buffer);
    }
//    sprintf(buffer,"%s°%s'%s°%s'",l.pos_lat_deg,l.pos_lat_min,l.pos_lon_deg,l.pos_lon_min);
//     display->setFont(font_12_alphanum);
//     display->drawUTF8(0, 54, pos.c_str());
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
    display->setCursor(0, 23);
    display->printf("当前时间");
    display->setFont(font_15_alphanum_bold);
    display->setCursor(display->getCursorX() + 3, display->getCursorY() - 1);
    uint16_t cx_prev = display->getCursorX();
    display->printf("%s", l.time);
    // drawEpi(l.type, 0, l.epi, cx_prev);
    directDrawEpi(getErrorCount(0, l.epi), cx_prev, l.time, 1, 1);
    // sprintf(buffer, "当前时间 %s ", l.time);
    // display->drawUTF8(0, 21, buffer);
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
    if (!display || !update || !enabled)
        return;

    if (l.type == 0)
        showLBJ0(l, r);
    else if (l.type == 1) {
        showLBJ1(l, r);
    } else if (l.type == 2) {
        showLBJ2(l, r);
    }
}

void ScreenWrapper::showLBJ(const struct lbj_data &l, const struct rx_info &r, const String &time_str, uint16_t lines,
                            uint32_t id, float temp) {
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

    char buffer[32];
    // show temp
    if (fabs(temp - 0.01) > 0.001) {
        sprintf(buffer, "%dC", (int) temp);
        display->drawStr(80, 7, buffer);
    }

    // show msg lines
    display->setDrawColor(0);
    display->drawBox(0, 56, 72, 8);
    display->setDrawColor(1);
    sprintf(buffer, "%04d,%u", lines, id);
    display->drawStr(0, 64, buffer);
    // if (!no_wifi) {
    //     String ipa = WiFi.localIP().toString();
    //     display->drawStr(0, 64, ipa.c_str());
    // } else
    //     display->drawStr(0, 64, "WIFI OFF");
    display->sendBuffer();

    // todo: change alphanumeric font.
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
    updateSleepTimestamp();
}

void ScreenWrapper::showSelectedLBJ(aPreferences *flash_cls, int8_t bias) {
    lbj_data lbj;
    rx_info rx;
    String rx_time;
    uint16_t line;
    uint32_t id;
    float temp;
    if (flash_cls->retrieve(&lbj, &rx, &rx_time, &line, &id, &temp, bias)) {
        showLBJ(lbj, rx, rx_time, line, id, temp);
    }
}

void ScreenWrapper::showListening() {
    if (!display)
        return;
    display->setFont(FONT_12_GB2312);
    display->setDrawColor(0);
    // display->drawBox(0, 42, 128, 14);
    display->drawBox(0, 8, 128, 48);
    display->drawBox(98, 0, 30, 8);
    display->setDrawColor(1);
    display->drawUTF8(0, 52, "正在监听...");
    display->sendBuffer();
}

void ScreenWrapper::clearTop(top_sectors sector, bool sendBuffer) {
    // if (!display)
    //     return;
    bool set_color = false;
    if (display->getDrawColor() != 0) {
        set_color = true;
        display->setDrawColor(0);
    }
    switch (sector) {
        case TOP_SECTOR_TIME:
            display->drawBox(0, 0, 79, 8);
            break;
        case TOP_SECTOR_TEMPERATURE:
            display->drawBox(80, 0, 98, 8);
            break;
        case TOP_SECTOR_RSSI:
            display->drawBox(99, 0, 128, 8);
            break;
        case TOP_SECTOR_ALL:
            display->drawBox(0, 0, 128, 8);
            break;
    }
    if (set_color)
        display->setDrawColor(1);
    if (sendBuffer)
        display->sendBuffer();
}

void ScreenWrapper::clearCenter(bool sendBuffer) {
    bool set_color = false;
    if (display->getDrawColor() != 0) {
        set_color = true;
        display->setDrawColor(0);
    }

    display->drawBox(0, 8, 128, 48);

    if (set_color)
        display->setDrawColor(1);
    if (sendBuffer)
        display->sendBuffer();
}

void ScreenWrapper::clearBottom(bottom_sectors sector, bool sendBuffer) {
    bool set_color = false;
    if (display->getDrawColor() != 0) {
        set_color = true;
        display->setDrawColor(0);
    }
    switch (sector) {
        case BOTTOM_SECTOR_IP:
            display->drawBox(0, 56, 72, 8);
            break;
        case BOTTOM_SECTOR_PPM:
            display->drawBox(73, 56, 15, 8); // 73->88
            break;
        case BOTTOM_SECTOR_IND:
            display->drawBox(89, 56, 6, 8); // 89->95
            break;
        case BOTTOM_SECTOR_CPU:
            display->drawBox(96, 56, 11, 8); // 96->107
            break;
        case BOTTOM_SECTOR_BAT:
            display->drawBox(108, 56, 20, 8); // 108->128
            break;
        case BOTTOM_SECTOR_ALL:
            display->drawBox(0, 56, 128, 8);
            break;
    }
    if (set_color)
        display->setDrawColor(1);
    if (sendBuffer)
        display->sendBuffer();
}

void ScreenWrapper::clearAll() {
    display->clearBuffer();
}

void ScreenWrapper::setFlash(aPreferences *flash_cls) {
    flash = flash_cls;
}

void ScreenWrapper::showSelectedLBJ(int8_t bias) {
    showSelectedLBJ(flash, bias);
}

void ScreenWrapper::showInfo(int8_t page) {
    if (!display)
        return;
    if (page > 3 || page < 1)
        return;
    String tokens[28];
    flash->retrieve(tokens, sizeof tokens / sizeof(String), 0);
    /* Standard format of cache:
     * 条目数,电压,系统时间,温度,日期,时间,type,train,direction,speed,position,time,info2_hex,loco_type,lbj_class,loco,route,
     * route_utf8,pos_lon_deg,pos_lon_min,pos_lat_deg,pos_lat_min,pos_lon,pos_lat,rssi,fer,ppm,id
     */
    clearAll();
    display->setFont(FONT_12_GB2312);
    display->drawUTF8(0, 12, "接收信息");
    char buffer[34];
    sprintf(buffer, "%d", page);
    display->drawUTF8(118, 12, buffer);
    display->drawHLine(0, 14, 128);

    switch (page) {
        case 1: {
            display->drawUTF8(0, 26, ("条目: " + tokens[0] + "," + tokens[27]).c_str());
            display->drawUTF8(0, 38, ("接收日期: " + tokens[4]).c_str());
            display->drawUTF8(0, 50, ("接收时间: " + tokens[5]).c_str());
            uint64_t time = std::stoull(tokens[2].c_str());
            display->drawUTF8(0, 62, ("系统时间: " + String(time / 1000) + " ms").c_str());
            break;
        }
        case 2: {
            display->drawUTF8(0, 26, ("电压:" + tokens[1] + "V 温度:" + tokens[3] + "C").c_str());
            float fer = std::stof(tokens[25].c_str());
            sprintf(buffer, "测量频偏: %.2f Hz", fer);
            display->drawUTF8(0, 38, buffer);
            fer = std::stof(tokens[26].c_str());
            sprintf(buffer, "设定频偏: %.2f ppm", fer);
            display->drawUTF8(0, 50, buffer);
            break;
        }
        case 3: {
            if (tokens[12].length()) {
                // display->drawUTF8(0,12,("I2HEX: "+tokens[12]).c_str());
                pword(("I2HEX: " + tokens[12]).c_str(), 0, 26);
            }
            break;
        }
        default:
            break;
    }
    display->sendBuffer();
}

void ScreenWrapper::pwordUTF8(const String &msg, int xloc, int yloc, int xmax, int ymax) {
    int Width = xmax - xloc;
    int Height = ymax - yloc;
    int StrW = display->getUTF8Width(msg.c_str());
    int8_t CharHeight = display->getMaxCharHeight();
    auto lines = Height / CharHeight;
    // Serial.printf("[D] lines %d, Height %d, CharH %d\n",lines,Height,CharHeight);

    String str = msg;
    for (int i = 0, j = yloc; i <= lines; ++i, j += CharHeight) {
        auto c = str.length();
        while (StrW > Width) {
            StrW = display->getUTF8Width(str.substring(0, c).c_str());
            --c;
        }
        // Serial.printf("[D] %d, %d\n",xloc,j);
        if (c == str.length()) {
            display->drawUTF8(xloc, j, str.substring(0, c).c_str());
            break;
        }
        display->drawUTF8(xloc, j, str.substring(0, c + 1).c_str());
        str = str.substring(c - 1, str.length());
        // Serial.println("[D] " + str);
    }

    // display->sendBuffer();
}

bool ScreenWrapper::isEnabled() const {
    return enabled;
}

void ScreenWrapper::setEnable(bool is_enable) {
    // if (is_enable)
    //     display->setPowerSave(false);
    display->setPowerSave(!is_enable);
    enabled = is_enable;
}

bool ScreenWrapper::isAutoSleep() const {
    return auto_sleep;
}

void ScreenWrapper::setSleep(bool is_sleep) {
    if (!auto_sleep || !enabled)
        return;
    display->setPowerSave(is_sleep);
    sleep = is_sleep;
}

bool ScreenWrapper::isSleep() const {
    return sleep;
}

void ScreenWrapper::autoSleep() {
    // if (!update_top || !update)
    //     return;
    if (millis64() - last_operation_time > AUTO_SLEEP_TIMEOUT && !isSleep()) {
        setSleep(true);
        // updateSleepTimestamp();
    }
}

void ScreenWrapper::updateSleepTimestamp() {
    last_operation_time = millis64();
}

void ScreenWrapper::drawEpi(int8_t error, uint16_t cx_prev, const String &str) {
    if (str.length() <= cx_prev)
        return;
    if (str[cx_prev] != '1' && str[cx_prev] != '2')
        return;
    uint16_t cx = display->getCursorX();
    uint16_t cy = display->getCursorY();
    if (error == 0) {
        if (str[cx_prev] == '1') {
            display->drawHLine(50, cy + 1, cx - 50);
        } else if (str[cx_prev] == '2') {
            // display->drawBox(50,cy+1,cx-50,display->getMaxCharHeight()+1);
            display->drawFrame(48, cy - display->getMaxCharHeight() + 3, cx - 47, display->getMaxCharHeight() - 1);
        }
    }
    display->setCursor(cx, cy);
}

void ScreenWrapper::drawEpi(int8_t type, int index, const String &epi, uint16_t cx_prev) {
    if (epi.length() <= index)
        return;
    if (epi[index] != '1' && epi[index] != '2')
        return;
    uint16_t cx = display->getCursorX();
    uint16_t cy = display->getCursorY();
    switch (type) {
        case 1: {
            if (index == 0) {
                if (epi[index] == '1') {
                    display->drawHLine(cx_prev, cy, cx - cx_prev);
                } else if (epi[index] == '2') {
                    display->drawFrame(cx_prev - 1, cy - display->getMaxCharHeight() + 3, cx - cx_prev + 2,
                                       display->getMaxCharHeight() - 2);
                }
            } else if (index == 1 || index == 2) {
                if (epi[index] == '1') {
                    display->drawHLine(cx_prev, cy, cx - cx_prev);
                } else if (epi[index] == '2') {
                    display->drawFrame(cx_prev - 2, cy - display->getMaxCharHeight() + 3, cx - cx_prev + 3,
                                       display->getMaxCharHeight() - 2);
                }
            } else if (index > 2 && index < 9) {
                if (epi[index] == '1') {
                    display->drawHLine(cx_prev, cy, cx - cx_prev);
                } else if (epi[index] == '2') {
                    display->drawFrame(cx_prev - 1, cy - display->getMaxCharHeight() + 3, cx - cx_prev + 1,
                                       display->getMaxCharHeight() - 2);
                }
            }
            break;
        }
        case 2: {
            if (epi[index] == '1') {
                display->drawHLine(cx_prev, cy + 1, cx - cx_prev);
            } else if (epi[index] == '2') {
                display->drawFrame(cx_prev - 2, cy - display->getMaxCharHeight() + 3, cx - cx_prev + 3,
                                   display->getMaxCharHeight() - 1);
            }
            break;
        }
        default:
            break;
    }
    display->setCursor(cx, cy);
}

int8_t ScreenWrapper::getErrorCount(int index, const String &epi) {
    if (epi.length() <= index)
        return -1;
    if (epi[index] != '1' && epi[index] != '2')
        return -1;
    return epi[index] - '0';
}

int8_t ScreenWrapper::getU8CharLen(uint8_t ch) {
    if (ch <= 0x7F)
        return 1;
    else if (ch >= 0xC0 && ch <= 0xDF)
        return 2;
    else if (ch >= 0xE0 && ch <= 0xEF)
        return 3;
    else if (ch >= 0xF0 && ch <= 0xF7)
        return 4;
    else if (ch >= 0xF8 && ch <= 0xFB)
        return 5;
    else if (ch >= 0xFC && ch <= 0xFD)
        return 6;
    return 0;
}

void ScreenWrapper::directDrawEpi(int8_t error, uint16_t cx_prev, const String &str, int y_offset, int xl, int xr) {
    if (error == -1 || !draw_epi)
        return;
    uint16_t cx = display->getCursorX();
    uint16_t cy = display->getCursorY();
    if (error == 1) {
        display->drawHLine(cx_prev, display->getCursorY() + y_offset, display->getCursorX() - cx_prev);
    } else if (error == 2) {
        // uint16_t cy_prev = display->getCursorY();
        display->drawBox(cx_prev - xl, display->getCursorY() - display->getMaxCharHeight() + 3,
                         display->getCursorX() - cx_prev + xr + xl, display->getMaxCharHeight() - (2 - y_offset));
        display->setDrawColor(0);
        display->setCursor(cx_prev, cy);
        display->print(str);
        // Serial.println(route_char);
        display->setDrawColor(1);
    }
    display->setCursor(cx, cy);
}
