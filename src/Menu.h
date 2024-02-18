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
    MENU_TF_SETTINGS,
    MENU_TF_CONFIRM,
    MENU_INDEX,
    MENU_INDEX_INPUT
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
    void showTFSettings(int16_t page);
    void highlightReadPPM(int8_t item);
    void alterDigitPPM(int8_t item, bool plus);
    void drawDigitPPM(int8_t item, bool inv = true, bool send = false);
    void confirmDigitAltPPM();
    void confirmUnmount();
    void showIndexFile();
    void highlightIndex(int8_t item);
    void alterDigitIndex(int8_t item, bool plus);
    void drawDigitIndex(int8_t item, bool inv = true, bool send = false);
    bool set_ppm = false;
    bool is_menu = false;
    bool is_msg = false;
    menu_pages menu_page = MENU_CLOSED;
    int16_t sub_page = 0;
    String items[10];
    int8_t selected_item = 0;
    int32_t bias;
    uint16_t lines;
    uint64_t update_timer = 0;
};


#endif //PAGER_RECEIVE_MENU_H
