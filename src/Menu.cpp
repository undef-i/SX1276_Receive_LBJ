//
// Created by FLN1021 on 2024/2/15.
//

#include "Menu.h"

void Menu::openMenu() {
    if (!display)
        return;
    update = false;
    if (always_new) {
        menu_page = MENU_SETTINGS;
        sub_page = -1;
        selected_item = 0;
        showSettings(sub_page);
        highlightItem(selected_item);
    } else {
        menu_page = MENU_RX_INFO;
        sub_page = 1;
        showInfo((int8_t) sub_page);
    }
    // if (!flash->isLatest(0)) {
    //     menu_page = MENU_RX_INFO;
    //     sub_page = 1;
    //     showInfo((int8_t) sub_page);
    // } else {
    //     menu_page = MENU_SETTINGS;
    //     sub_page = -1;
    //     selected_item = 0;
    //     showSettings(sub_page);
    //     highlightItem(selected_item);
    // }
    is_menu = true;
    // display->sendBuffer();

}

bool Menu::isMenu() const {
    return is_menu;
}

void Menu::closeMenu() {
    if (!display)
        return;
    if (menu_page > MENU_SETTINGS) {
        showLast();
        return;
    }
    clearAll();
    update = true;
    display->setFont(u8g2_font_squeezed_b7_tr);
    updateInfo();
    if (always_new) {
        showListening(); // todo: add show listening while idle.
    } else
        showSelectedLBJ(0);
    // if (!first_rx) {
    //     if (!always_new) {
    //         showSelectedLBJ(0);
    //     } else
    //         showListening();
    // } else
    //     showSelectedLBJ(0);
    menu_page = MENU_CLOSED;
    sub_page = 0;
    is_menu = false;
    // if (always_new)
    //     resumeUpdate();
}

void Menu::handleKey(bool up) {
    if (!display)
        return;
    if (!is_menu)
        return;
    if (is_msg)
        return;
    switch (menu_page) {
        case MENU_CLOSED:
            break;
        case MENU_RX_INFO:
            if (up)
                sub_page--;
            else
                sub_page++;
            if (sub_page < 1) {
                menu_page = MENU_SETTINGS;
                sub_page = -1;
                selected_item = 4;
            } else {
                if (sub_page > 3)
                    sub_page = 1;
                showInfo((int8_t) sub_page);
                break;
            }
        case MENU_SETTINGS:
            if (up)
                selected_item--;
            else
                selected_item++;
            if (selected_item < 0) {
                sub_page--;
                selected_item = 3;
            }
            if (selected_item > 3) {
                sub_page++;
                selected_item = 0;
            }

            if (sub_page > -1) {
                if (always_new) {
                    sub_page = -2;
                } else {
                    menu_page = MENU_RX_INFO;
                    sub_page = 1;
                    showInfo((int8_t) sub_page);
                    break;
                }
            }
            if (sub_page < -2)
                sub_page = -1;
            showSettings(sub_page);
            highlightItem(selected_item);
            break;
        case MENU_ABOUT:
            if (up)
                sub_page--;
            else
                sub_page++;
            if (sub_page < 1)
                sub_page = 2;
            if (sub_page > 2)
                sub_page = 1;
            showAbout(sub_page);
            break;
        case MENU_FREQ:
            if (up)
                selected_item--;
            else
                selected_item++;
            if (selected_item < 0) {
                selected_item = 1;
            }
            if (selected_item > 1) {
                selected_item = 0;
            }
            showFreq();
            highlightItem(selected_item);
            break;
        case MENU_FREQ_READ_PPM:
            if (up)
                selected_item--;
            else
                selected_item++;
            if (selected_item < 0) {
                selected_item = 6;
            }
            if (selected_item > 6) {
                selected_item = 0;
            }
            // Serial.printf("[D] sel_item = %d\n",selected_item);
            showReadPPM();
            // Serial.printf("[D] sel_item1 = %d\n",selected_item);
            highlightReadPPM(selected_item);
            break;
        case MENU_FREQ_PPM_SET:
            alterDigitPPM(selected_item, up);
            break;
        case MENU_TF_SETTINGS:
            if (up)
                sub_page--;
            else
                sub_page++;
            if (sub_page > 2)
                sub_page = 1;
            if (sub_page < 1)
                sub_page = 2;
            // if (selected_item < 0) {
            //     selected_item = 6;
            // }
            // if (selected_item > 6) {
            //     selected_item = 0;
            // }
            showTFSettings(sub_page);
            if (sub_page == 1) {
                if (selected_item != 0)
                    selected_item = 0;
                highlightItem(selected_item);
            }

            break;
        case MENU_TF_CONFIRM:
            if (up)
                selected_item--;
            else
                selected_item++;
            if (selected_item < 0) {
                selected_item = 1;
            }
            if (selected_item > 1) {
                selected_item = 0;
            }
            if (selected_item == 1) {
                confirmUnmount();
                display->drawBox(64, 49, 51, 12);
                display->setDrawColor(0);
                display->drawUTF8(76, 60, "取消");
                display->setDrawColor(1);
                display->sendBuffer();
            } else {
                confirmUnmount();
                display->drawBox(13, 49, 51, 12);
                display->setDrawColor(0);
                display->drawUTF8(26, 60, "确定");
                display->setDrawColor(1);
                display->sendBuffer();
            }
            break;
        case MENU_INDEX:
            if (up)
                selected_item--;
            else
                selected_item++;
            if (selected_item < 0) {
                selected_item = 5;
            }
            if (selected_item > 5) {
                selected_item = 0;
            }
            showIndexFile();
            highlightIndex(selected_item);
            break;
        case MENU_INDEX_INPUT:
            alterDigitIndex(selected_item, up);
            break;
        case MENU_OLED:
            if (up)
                selected_item--;
            else
                selected_item++;
            if (selected_item < 0) {
                selected_item = 1;
            }
            if (selected_item > 1) {
                selected_item = 0;
            }
            showOLED();
            highlightItem(selected_item);
            // more coming...
            break;
    }
}

void Menu::showSettings(int16_t page) {
    if (!display)
        return;
    clearAll();
    display->setFont(FONT_12_GB2312);
    display->drawUTF8(0, 12, "系统设置");
    char buffer[34];
    sprintf(buffer, "%d", page);
    display->drawUTF8(118, 12, buffer);
    display->drawHLine(0, 14, 128);

    switch (page) {
        case -1: {
            items[0] = "频率校正";
            items[1] = "TF卡设置";
            items[2] = "索引条目";
            items[3] = "关于";
            for (int i = 0, c = 26; i < 4; ++i, c += 12) {
                display->drawUTF8(0, c, items[i].c_str());
            }
            break;
        }
        case -2: {
            items[0] = "预留条目";
            items[1] = "显示设置";
            items[2] = "RTC信息";
            items[3] = "系统信息";
            for (int i = 0, c = 26; i < 4; ++i, c += 12) {
                display->drawUTF8(0, c, items[i].c_str());
            }
            break;
        }
        default:
            break;
    }

    // display->sendBuffer();
}

void Menu::highlightItem(int8_t item) {
    display->drawBox(0, 16 + item * 12, 128, 12);
    display->setDrawColor(0);
    display->drawUTF8(0, 26 + item * 12, items[item].c_str());
    display->setDrawColor(1);
    display->sendBuffer();
}

void Menu::acknowledge() {
    if (!display)
        return;
    if (is_msg) {
        showLast();
        return;
    }
    switch (menu_page) {
        case MENU_SETTINGS:
            if (sub_page == -1) {
                switch (selected_item) {
                    case 0:
                        menu_page = MENU_FREQ;
                        showFreq();
                        selected_item = 0;
                        highlightItem(selected_item);
                        break;
                    case 1:
                        menu_page = MENU_TF_SETTINGS;
                        sub_page = 1;
                        showTFSettings(sub_page);
                        selected_item = 0;
                        highlightItem(selected_item);
                        break;
                    case 2:
                        menu_page = MENU_INDEX;
                        lines = flash->getRetLines();
                        showIndexFile();
                        selected_item = 0;
                        highlightIndex(selected_item);
                        break;
                    case 3:
                        menu_page = MENU_ABOUT;
                        sub_page = 1;
                        showAbout(1);
                        break;
                    default:
                        break;
                }
            } else if (sub_page == -2) {
                switch (selected_item) {
                    case 1:
                        menu_page = MENU_OLED;
                        showOLED();
                        selected_item = 0;
                        highlightItem(selected_item);
                        break;
                    default:
                        break;
                }
            }
            break;
        case MENU_FREQ:
            switch (selected_item) {
                case 0: {
                    if (freq_correction) {
                        prb_count = 0;
                        prb_timer = 0;
                        car_count = 0;
                        car_timer = 0;
                        freq_correction = false;
                        showFreq();
                        highlightItem(selected_item);
                        Serial.println("$ Frequency Correction Disabled");
                    } else {
                        freq_correction = true;
                        showFreq();
                        highlightItem(selected_item);
                        Serial.println("$ Frequency Correction Enabled");
                    }
                    break;
                }
                case 1: {
                    menu_page = MENU_FREQ_READ_PPM;
                    if (freq_correction) {
                        showMessage("信息", "调整频偏需禁用自动频率校正");
                        break;
                    }
                    selected_item = 0;
                    bias = (int32_t)(getBias(actual_frequency) * 100);
                    showReadPPM();
                    highlightReadPPM(selected_item);
                    break;
                }
            }
            break;
        case MENU_FREQ_READ_PPM: {

            if (selected_item < 5) {
                //     display->drawBox(19 + selected_item * 16, 22, 14, 22);
                //     display->setDrawColor(0);
                //     display->drawStr(20 + selected_item * 16, 40, items[selected_item].c_str());
                //     display->setDrawColor(1);
                //     display->sendBuffer();
                //     menu_page = MENU_FREQ_PPM_SET;
                // } else if (selected_item < 5) {
                //     display->drawBox(23 + selected_item * 16, 22, 14, 22);
                //     display->setDrawColor(0);
                //     display->drawStr(24 + selected_item * 16, 40, items[selected_item].c_str());
                //     display->setDrawColor(1);
                //     display->sendBuffer();
                //     menu_page = MENU_FREQ_PPM_SET;
                drawDigitPPM(selected_item, true, true);
                menu_page = MENU_FREQ_PPM_SET;

            } else if (selected_item == 5) {
                // confirm
                set_ppm = true;
                ppm = (float) (bias * 0.01);
                showLast();
            } else if (selected_item == 6) {
                // cancel
                showLast();
            }
            break;
        }
        case MENU_FREQ_PPM_SET:
            confirmDigitAltPPM();
            drawDigitPPM(selected_item, false, true);
            highlightReadPPM(selected_item);
            menu_page = MENU_FREQ_READ_PPM;
            break;
        case MENU_TF_SETTINGS:
            if (sub_page == 1 && selected_item == 0) {
                if (!sd1.status()) {
                    // mount
                    items[0] = "正在挂载SD...";
                    highlightItem(0);
                    Serial.println("[SDLOG] issued SD reopen.");
                    SD_LOG::reopenSD();
                    sd1.begin("/LOGTEST");
                    esp_task_wdt_reset();
                    sd1.beginCSV("/CSVTEST");
                    sd1.append("[SDLOG] SD卡已重新挂载\n");
                    Serial.println("$ [SDLOG] SD reopen.");
                } else {
                    // unmount
                    menu_page = MENU_TF_CONFIRM;
                    confirmUnmount();
                    selected_item = 1;
                    display->drawBox(64, 49, 51, 12);
                    display->setDrawColor(0);
                    display->drawUTF8(76, 60, "取消");
                    display->setDrawColor(1);
                    display->sendBuffer();
                    // Serial.println("[SDLOG] issued SD end.");
                    // sd1.append("[SDLOG] SD卡将被卸载\n");
                    // sd1.end();
                    // Serial.println("$ [SDLOG] SD end.");
                }
            }
            break;
        case MENU_TF_CONFIRM:
            if (selected_item) {
                showLast();
            } else {
                showLast();
                Serial.println("[SDLOG] issued SD end.");
                sd1.append("[SDLOG] SD卡将被卸载\n");
                sd1.end();
                Serial.println("$ [SDLOG] SD end.");
            }
            break;
        case MENU_INDEX:
            if (selected_item < 4) {
                drawDigitIndex(selected_item, true, true);
                menu_page = MENU_INDEX_INPUT;
            } else if (selected_item == 4) {
                // acknowledge
                flash->setRetLines(lines);
                menu_page = MENU_RX_INFO;
                closeMenu();
            } else if (selected_item == 5) {
                showLast();
            }
            break;
        case MENU_INDEX_INPUT:
            // confirmDigitAltPPM();
            lines = std::stoi((items[0] + items[1] + items[2] + items[3]).c_str());
            if (lines > PREF_MAX_LINES)
                lines = PREF_MAX_LINES;
            if (lines > flash->getID())
                lines = flash->getID();
            drawDigitIndex(selected_item, false);
            highlightIndex(selected_item);
            menu_page = MENU_INDEX;
            break;
        case MENU_OLED:
            switch (selected_item) {
                case 0:
                    // clearAll();
                    // display->sendBuffer();
                    setEnable(false);
                    // display->setPowerSave(true);
                    menu_page = MENU_CLOSED;
                    sub_page = 0;
                    update = true;
                    is_menu = false;
                    // enabled = false;
                    break;
                case 1:
                    draw_epi = !draw_epi;
                    showOLED();
                    highlightItem(selected_item);
                    break;
            }
            break;
        default:
            return;
    }
}

void Menu::showLast() {
    if (is_msg)
        is_msg = false;
    switch (menu_page) {
        case MENU_ABOUT:
            sub_page = -1;
            showSettings(sub_page);
            selected_item = 3;
            highlightItem(selected_item);
            menu_page = MENU_SETTINGS;
            break;
        case MENU_FREQ:
            showSettings(-1);
            selected_item = 0;
            highlightItem(selected_item);
            menu_page = MENU_SETTINGS;
            break;
        case MENU_FREQ_READ_PPM:
            showFreq();
            selected_item = 1;
            highlightItem(selected_item);
            menu_page = MENU_FREQ;
            break;
        case MENU_FREQ_PPM_SET:
            showReadPPM();
            highlightReadPPM(selected_item);
            menu_page = MENU_FREQ_READ_PPM;
            break;
        case MENU_TF_SETTINGS:
            sub_page = -1;
            showSettings(sub_page);
            selected_item = 0;
            highlightItem(selected_item);
            menu_page = MENU_SETTINGS;
            break;
        case MENU_TF_CONFIRM:
            selected_item = 0;
            sub_page = 1;
            showTFSettings(sub_page);
            highlightItem(selected_item);
            menu_page = MENU_TF_SETTINGS;
            break;
        case MENU_INDEX:
            selected_item = 2;
            sub_page = -1;
            showSettings(sub_page);
            highlightItem(selected_item);
            menu_page = MENU_SETTINGS;
            break;
        case MENU_INDEX_INPUT:
            showIndexFile();
            highlightIndex(selected_item);
            menu_page = MENU_INDEX;
            break;
        case MENU_OLED:
            sub_page = -2;
            showSettings(sub_page);
            selected_item = 1;
            highlightItem(selected_item);
            menu_page = MENU_SETTINGS;
            break;
        default:
            return;
    }
}

void Menu::showAbout(int16_t page) {
    clearAll();
    display->setFont(FONT_12_GB2312);
    display->drawUTF8(0, 12, "关于");
    // char buffer[34];
    // sprintf(buffer, "%d", page);
    // display->drawUTF8(118, 12, buffer);
    display->drawHLine(0, 14, 128);
    display->drawUTF8(118, 12, String(page).c_str());

    char buffer[32];
    if (page == 1) {
        sprintf(buffer, "SX1276_RX_LBJ 项目");
        display->drawUTF8(0, 26, buffer);
        sprintf(buffer, "基于ESP32-Arduino");
        display->drawUTF8(0, 38, buffer);
        const esp_partition_t *p = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                                            ESP_PARTITION_SUBTYPE_APP_OTA_0, "app0");
        uint8_t sha256[32];
        esp_partition_get_sha256(p, sha256);
        String hash;
        for (unsigned char i: sha256) {
            char buf[4];
            sprintf(buf, "%02x", i);
            hash += buf;
        }
        sprintf(buffer, "版本: %s", hash.substring(0, 16).c_str());
        display->drawUTF8(0, 50, buffer);
        sprintf(buffer, "构建日期: %s", __DATE__);
        display->drawUTF8(0, 62, buffer);

    } else if (page == 2) {
        sprintf(buffer, "构建时间: %s", __TIME__);
        display->drawUTF8(0, 26, buffer);
    }

    display->sendBuffer();
}

void Menu::showFreq(bool send) {
    clearAll();
    // display->setDrawColor(0);
    // display->drawBox(0,0,128,64);
    // display->setDrawColor(1);
    display->setFont(FONT_12_GB2312);
    display->drawUTF8(0, 12, "频率校正");
    display->drawHLine(0, 14, 128);

    items[0] = "自动频率校正 ";
    if (freq_correction)
        items[0] += "[已启用]";
    else
        items[0] += "[已禁用]";
    items[1] = "频率偏移       ";
    items[1] += "[" + String(getBias(actual_frequency)) + "]";
    for (int i = 0, c = 26; i < 2; ++i, c += 12) {
        display->drawUTF8(0, c, items[i].c_str());
    }
    if (send)
        display->sendBuffer();
}

void Menu::showReadPPM() {
    display->setDrawColor(0);
    display->drawBox(12, 4, 104, 58);
    display->setDrawColor(1);
    display->drawFrame(12, 4, 104, 58);
    display->drawUTF8(15, 16, "频率偏移[PPM]");
    display->drawHLine(14, 18, 100);


    uint8_t digits[5];
    // bias = (int32_t) (getBias(actual_frequency) * 100);
    uint32_t a_bias = abs(bias);
    digits[0] = bias >= 0 ? 1 : 0;
    digits[1] = (a_bias / 1000) % 10;
    digits[2] = (a_bias / 100) % 10;
    digits[3] = (a_bias / 10) % 10;
    digits[4] = a_bias % 10;

    display->setFont(u8g2_font_spleen12x24_mn);
    for (int i = 0, c = 20; i < sizeof digits; ++i, c += 16) {
        char buffer[2];
        if (i == 0) {
            digits[0] ? sprintf(buffer, "+") : sprintf(buffer, "-");
            display->drawStr(c, 40, buffer);
            items[i] = buffer;
            continue;
        }
        if (i == 3) {
            c -= 6;
            display->drawStr(c, 40, ".");
            c += 10;
        }
        sprintf(buffer, "%1d", digits[i]);
        items[i] = buffer;
        display->drawStr(c, 40, buffer);
    }
    display->setFont(FONT_12_GB2312);

    display->drawHLine(14, 48, 100);
    display->drawVLine(64, 50, 10);
    display->drawUTF8(26, 60, "确定");
    display->drawUTF8(76, 60, "取消");
    // display->sendBuffer();
}

void Menu::highlightReadPPM(int8_t item) {
    // Serial.printf("[D] item = %d\n",item);
    display->setDrawColor(1);
    if (item == 5) {
        display->drawBox(13, 49, 51, 12);
        display->setDrawColor(0);
        display->drawUTF8(26, 60, "确定");
        display->setDrawColor(1);
    } else if (item == 6) {
        display->drawBox(64, 49, 51, 12);
        display->setDrawColor(0);
        display->drawUTF8(76, 60, "取消");
        display->setDrawColor(1);
    } else if (item > 2) {
        display->drawFrame(23 + item * 16, 22, 14, 22);
    } else {
        display->drawFrame(19 + item * 16, 22, 14, 22);
    }
    display->sendBuffer();
}

void Menu::alterDigitPPM(int8_t item, bool plus) {
    display->setFont(u8g2_font_spleen12x24_mn);
    if (item == 0) {
        if (items[item] == "+") {
            items[item] = "-";
        } else {
            items[item] = "+";
        }
        display->drawBox(19, 22, 14, 22);
        display->setDrawColor(0);
        display->drawStr(20, 40, items[item].c_str());
        display->setDrawColor(1);
        display->sendBuffer();
        return;
    }
    int8_t digit = (int8_t) std::stoi(items[item].c_str());
    plus ? digit++ : digit--;
    if (digit > 9)
        digit = 0;
    if (digit < 0)
        digit = 9;
    char buf[2];
    sprintf(buf, "%1d", digit);
    items[item] = buf;
    drawDigitPPM(item);
    display->sendBuffer();
}

void Menu::drawDigitPPM(int8_t item, bool inv, bool send) {
    display->setFont(u8g2_font_spleen12x24_mn);
    if (item == 0) {
        display->setDrawColor(inv); // 0 default
        display->drawBox(19, 22, 14, 22);
        display->setDrawColor(!inv);
        if (items[item] == "+") {
            display->drawStr(20, 40, "+");
        } else {
            display->drawStr(20, 40, "-");
        }
    } else if (item < 3) {
        display->setDrawColor(inv); // 0 default
        display->drawBox(19 + item * 16, 22, 14, 22);
        display->setDrawColor(!inv);
        display->drawStr(20 + item * 16, 40, items[item].c_str());
    } else if (item < 5) {
        display->setDrawColor(inv);
        display->drawBox(23 + item * 16, 22, 14, 22);
        display->setDrawColor(!inv);
        display->drawStr(24 + item * 16, 40, items[item].c_str());
    }
    display->setDrawColor(1);
    if (send)
        display->sendBuffer();
    display->setFont(FONT_12_GB2312);
}

void Menu::confirmDigitAltPPM() {
    if (selected_item > 4)
        return;
    uint8_t digits[5];
    uint32_t a_bias = abs(bias);
    digits[0] = bias >= 0 ? 1 : 0;
    digits[1] = (a_bias / 1000) % 10;
    digits[2] = (a_bias / 100) % 10;
    digits[3] = (a_bias / 10) % 10;
    digits[4] = a_bias % 10;

    if (selected_item == 0) {
        if (items[selected_item] == "+")
            digits[0] = 1;
        else
            digits[0] = 0;
    } else {
        digits[selected_item] = std::stoi(items[selected_item].c_str());
    }

    bias = digits[1] * 1000 + digits[2] * 100 + digits[3] * 10 + digits[4];
    if (!digits[0])
        bias = bias * (-1);

    // Serial.printf("[D] bias %d\n",bias);
}

bool Menu::ppmChanged() const {
    return set_ppm;
}

void Menu::clearPPMFlag() {
    set_ppm = false;
}

void Menu::showMessage(const char *title, const char *message) {
    is_msg = true;
    display->setFont(FONT_12_GB2312);
    display->setDrawColor(0);
    display->drawBox(12, 4, 104, 58);
    display->setDrawColor(1);
    display->drawFrame(12, 4, 104, 58);
    display->drawUTF8(15, 16, title);
    display->drawHLine(14, 18, 100);

    // display->setCursor(18,34);
    // display->printf("%s", message);
    // display->drawUTF8(18,34,message);
    pwordUTF8(String(message), 18, 34, 100, 58);

    display->sendBuffer();
}

void Menu::updatePage() {
    if (!is_menu)
        return;
    if (millis64() - update_timer < 30)
        return;

    switch (menu_page) {
        case MENU_FREQ:
            showFreq();
            highlightItem(selected_item);
            break;
        case MENU_TF_SETTINGS:
            if (sub_page != 1)
                return;
            showTFSettings(sub_page);
            highlightItem(selected_item);
            break;
        default:
            return;
    }

    update_timer = millis64();
}

void Menu::showTFSettings(int16_t page) {
    clearAll();
    display->setFont(FONT_12_GB2312);
    display->drawUTF8(0, 12, "TF卡设置");
    display->drawHLine(0, 14, 128);

    if (!sd1.status()) {
        if (sub_page != 1)
            sub_page = 1;
        items[0] = "装载TF卡";
        display->drawUTF8(0, 26, items[0].c_str());
        display->drawUTF8(0, 38, "未插入TF卡");
        return;
    }

    display->drawUTF8(118, 12, String(page).c_str());
    switch (page) {
        case 1: {
            items[0] = "卸载TF卡";
            display->drawUTF8(0, 26, items[0].c_str());
            char buffer[64];
            sprintf(buffer, "已使用: %02.2f/%02.2f GB", (double) SD.usedBytes() / (1024 * 1024 * 1024),
                    (double) SD.totalBytes() / (1024 * 1024 * 1024));
            display->drawUTF8(0, 38, buffer);
            display->drawUTF8(0, 50, ("日志: " + sd1.retFilename(SDLOG_FILE_LOG)).c_str());
            display->drawUTF8(0, 62, ("CSV: " + sd1.retFilename(SDLOG_FILE_CSV)).c_str());
            break;
        }
        case 2:
            if (sd1.retFilename(SDLOG_FILE_CD).length()) {
                display->drawUTF8(0, 26, ("转储文件: " + sd1.retFilename(SDLOG_FILE_CD)).c_str());
            } else {
                display->drawUTF8(0, 26, "无转储文件");
            }
            display->sendBuffer();
            break;
        default:
            return;
    }

}

void Menu::confirmUnmount() {
    display->setDrawColor(0);
    display->drawBox(12, 4, 104, 58);
    display->setDrawColor(1);
    display->drawFrame(12, 4, 104, 58);
    display->drawUTF8(15, 16, "卸载TF卡");
    display->drawHLine(14, 18, 100);

    display->drawUTF8(18, 34, "是否卸载TF卡?");

    display->drawHLine(14, 48, 100);
    display->drawVLine(64, 50, 10);
    display->drawUTF8(26, 60, "确定");
    display->drawUTF8(76, 60, "取消");
}

void Menu::showIndexFile() {
    display->setDrawColor(0);
    display->drawBox(12, 4, 104, 58);
    display->setDrawColor(1);
    display->drawFrame(12, 4, 104, 58);
    display->drawUTF8(15, 16, "相对索引编号");
    display->drawHLine(14, 18, 100);

    display->setFont(u8g2_font_spleen12x24_mn);
    // uint16_t lines = flash->getRetLines();
    for (int i = 0, j = 1000, k = 30; i < 4; ++i, j = j / 10, k += 16) {
        uint8_t digit = (lines / j) % 10;
        char buffer[2];
        sprintf(buffer, "%1d", digit);
        items[i] = buffer;
        display->drawStr(k, 40, buffer);
    }
    display->setFont(FONT_12_GB2312);

    display->drawHLine(14, 48, 100);
    display->drawVLine(64, 50, 10);
    display->drawUTF8(26, 60, "确定");
    display->drawUTF8(76, 60, "取消");
}

void Menu::highlightIndex(int8_t item) {
    display->setDrawColor(1);
    if (item == 4) {
        display->drawBox(13, 49, 51, 12);
        display->setDrawColor(0);
        display->drawUTF8(26, 60, "确定");
        display->setDrawColor(1);
    } else if (item == 5) {
        display->drawBox(64, 49, 51, 12);
        display->setDrawColor(0);
        display->drawUTF8(76, 60, "取消");
        display->setDrawColor(1);
    } else {
        display->drawFrame(29 + item * 16, 22, 14, 22);
    }
    display->sendBuffer();
}

void Menu::alterDigitIndex(int8_t item, bool plus) {
    // display->setFont(u8g2_font_spleen12x24_mn);
    // item 0,1,2,3
    // uint16_t div = 1000;
    int digits[4];
    uint8_t dmax = 9;
    // for (int i = 0, div = 1000; i < 4; ++i, div /= 10) {
    //     digits[i] = (lines / div) % 10;
    // }
    for (int i = 0; i < 4; ++i) {
        digits[i] = std::stoi(items[i].c_str());
    }
    if (item == 0)
        dmax = (PREF_MAX_LINES / 1000) % 10;
    else if (item == 1 && digits[0] == (PREF_MAX_LINES / 1000) % 10)
        dmax = (PREF_MAX_LINES / 100) % 10;
    else if (item == 2 && digits[0] == (PREF_MAX_LINES / 1000) % 10 && digits[1] == (PREF_MAX_LINES / 100) % 10)
        dmax = (PREF_MAX_LINES / 10) % 10;
    else if (item == 3 && digits[0] == (PREF_MAX_LINES / 1000) % 10 && digits[1] == (PREF_MAX_LINES / 100) % 10 &&
             digits[2] == (PREF_MAX_LINES / 10) % 10)
        dmax = PREF_MAX_LINES % 10;

    plus ? digits[item]++ : digits[item]--;
    Serial.printf("digits %d\n", digits[item]);
    if (digits[item] > dmax)
        digits[item] = 0;
    else if (digits[item] < 0)
        digits[item] = dmax;
    Serial.printf("digits %d\n", digits[item]);
    char buffer[2];
    sprintf(buffer, "%1d", digits[item]);
    items[item] = buffer;
    drawDigitIndex(item);
    // display->setFont(FONT_12_GB2312);
    display->sendBuffer();
}

void Menu::drawDigitIndex(int8_t item, bool inv, bool send) {
    display->setFont(u8g2_font_spleen12x24_mn);
    display->setDrawColor(inv); // 0 default
    display->drawBox(29 + item * 16, 22, 14, 22);
    display->setDrawColor(!inv);
    display->drawStr(30 + item * 16, 40, items[item].c_str());
    if (send)
        display->sendBuffer();
    display->setFont(FONT_12_GB2312);
}

void Menu::showOLED() {
    clearAll();
    display->setFont(FONT_12_GB2312);
    display->drawUTF8(0, 12, "显示设置");
    display->drawHLine(0, 14, 128);

    items[0] = "关闭显示";
    display->drawUTF8(0, 26, items[0].c_str());
    items[1] = "显示纠错位置 ";
    if (draw_epi)
        items[1] += "[已启用]";
    else
        items[1] += "[已禁用]";
    display->drawUTF8(0, 38, items[1].c_str());
}


