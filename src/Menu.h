//
// Created by FLN1021 on 2024/2/15.
//

#ifndef PAGER_RECEIVE_MENU_H
#define PAGER_RECEIVE_MENU_H
#include "ScreenWrapper.h"

enum menu_pages {
    MENU_CLOSED,
    MENU_RX_INFO,
    MENU_SETTINGS,
    MENU_ABOUT,
    MENU_FREQ,
    MENU_FREQ_READ_PPM,
    MENU_FREQ_PPM_SET,
    MENU_STORAGE_INFO,
    MENU_STORAGE_SETTINGS,
    MENU_INDEX,
    MENU_INDEX_INPUT,
    MENU_OLED,
    MENU_TX_TEST
};

class Menu : public ScreenWrapper {
public:
    void openMenu();
    bool isMenu() const;
    void closeMenu();
    void handleKey(bool up);
    void acknowledge();
    bool ppmChanged() const;
    void clearPPMFlag();
    void showMessage(const char *title, const char *message);
    void updatePage();

private:
    void showSettings(int16_t page);
    void highlightItem(int8_t item);
    void showAbout(int16_t page);
    void showLast();
    void showFreq(bool send = false);
    void showReadPPM();
    void showStorageInfo(int16_t page);
    void highlightReadPPM(int8_t item);
    void alterDigitPPM(int8_t item, bool plus);
    void drawDigitPPM(int8_t item, bool inv = true, bool send = false);
    void confirmDigitAltPPM();
    void showIndexFile();
    // 内部存储功能
    void handleStorageInfoDisplay();   // 显示存储使用情况
    void handleStorageMonitoring();    // 显示存储状态监控
    void handleStorageLimitSettings(); // 存储限制设置
    void highlightIndex(int8_t item);
    void alterDigitIndex(int8_t item, bool plus);
    void drawDigitIndex(int8_t item, bool inv = true, bool send = false);
    void showOLED();
    void showTxTest();
    bool set_ppm = false;
    bool is_menu = false;
    bool is_msg = false;
    menu_pages menu_page = MENU_CLOSED;
    int16_t sub_page = 0;
    String items[10];
    int8_t selected_item = 0;
    int32_t bias = 0;
    uint16_t lines = 0;
    uint64_t update_timer = 0;
};


#endif //PAGER_RECEIVE_MENU_H
