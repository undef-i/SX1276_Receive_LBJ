/*
   SX1276 LBJ Message Receive Project
   Migrated by FLN1021 on Sep 2023.
   Modified from RadioLib Pager (POCSAG) Receive Example.
   Original file:
   https://github.com/jgromes/RadioLib/blob/master/examples/Pager/Pager_Receive/Pager_Receive.ino
*/

#include <atomic>
/*
   RadioLib Pager (POCSAG) Receive Example

   This example shows how to receive FSK packets without using
   SX127x packet engine.

   This example receives POCSAG messages using SX1278's【
   FSK modem in direct mode.

   Other modules that can be used to receive POCSAG:
    - SX127x/RFM9x
    - RF69
    - SX1231
    - CC1101
    - Si443x/RFM2x

   For default module settings, see the wiki page
   https://github.com/jgromes/RadioLib/wiki/Default-configuration#sx127xrfm9x---lora-modem

   For full API reference, see the GitHub Pages
   https://jgromes.github.io/RadioLib/
*/
#pragma execution_character_set("utf-8")

// include libraries
#include <RadioLib.h>
#include "coredump.h"
#include "customfont.h"
#include <esp_task_wdt.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLECharacteristic.h>
#include <ArduinoJson.h>

// configure the wifi connection
String wifiSSID = "MI CC9 Pro";
String wifiPassword = "11223344";
#define NETWORK_TIMEOUT 1800000 // 30 minutes

#define WDT_TIMEOUT 20 // sec
// #define WDT_RST_PERIOD 4000 // ms
#define FD_TASK_STACK_SIZE 8000
#define FD_TASK_TIMEOUT 750 // ms
#define FD_TASK_ATTEMPTS 3
#define LED_ON_TIME 200 // ms
//region Variables
SX1276 radio = new Module(RADIO_CS_PIN, RADIO_DIO0_PIN, RADIO_RST_PIN, RADIO_DIO1_PIN);
// receiving packets requires connection
// to the module direct output pin
const int pin = RADIO_BUSY_PIN;
float rssi_cache = 0;
// float fer = 0;
float fers[32]{};
float actual_frequency = 0;
float freq_last = 0;
float car_fer_last = 0;
// create Pager client instance using the FSK module
PagerClient pager(&radio);
// timers
uint64_t format_task_timer = 0;
uint64_t runtime_timer = 0;
uint64_t screen_timer = 0;
uint64_t led_timer = 0;
uint64_t timer4 = 0;
// uint64_t wdt_timer = 0;
uint64_t net_timer = 0;
uint64_t prb_timer = 0;
uint64_t car_timer = 0;
uint32_t prb_count = 0;
uint32_t car_count = 0;
uint32_t ip_last = 0;
float ppm = INITIAL_PPM;



#define SERVICE_UUID           "0000FFE0-0000-1000-8000-00805F9B34FB"
#define CHARACTERISTIC_UUID_TX "0000FFE1-0000-1000-8000-00805F9B34FB"
#define BLE_DEVICE_NAME        "LBJReceiver"

BLEServer *pServer = nullptr;
BLECharacteristic *pTxCharacteristic = nullptr;
std::atomic<bool> deviceConnected{false};
bool oldDeviceConnected = false;
uint64_t ble_timer = 0;

struct lbj_data current_lbj_data;
SemaphoreHandle_t data_mutex = nullptr;

void sendTrainDataOverBLE(const struct lbj_data &l, const struct rx_info &r, bool isTest = false);
void readCsvAndSendBLE();

inline float actualFreq(float bias) {
    actual_frequency = (float) ((TARGET_FREQ * bias) / 1e6 + TARGET_FREQ);
    return actual_frequency;
}

bool freq_correction = AFC_ENABLE;
// bool bandwidth_altered = false;
bool is_startline = true;
bool exec_init_f80 = false;
// bool agc_triggered = false;
bool low_volt_warned = false;
bool oled_off = false;
bool give_tel_rssi = false;
bool give_tel_gain = false;
bool tel_set_ppm = false;
bool no_wifi = true;
bool have_cd = false;
bool telnet_online = false;
bool ble_enabled = true;
SD_LOG sd1;

class SafeServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) override {
      deviceConnected.store(true, std::memory_order_release);
      Serial.println("[BLE] Device is connected");
      ble_timer = millis64();
    }
    
    void onDisconnect(BLEServer* pServer) override {
      deviceConnected.store(false, std::memory_order_release);
      Serial.println("[BLE] Device is disconnected");
    }
};
struct rx_info rxInfo;
struct data_bond *db = nullptr;
QueueHandle_t xQueue_db;
// PagerClient::pocsag_data *pd = nullptr;
//endregion

//region Functions
// 将电池电压转换为电量百分比
int voltageToPercentage(float voltage) {
    // 假设锂电池的工作电压范围为3.0V-4.2V
    if (voltage >= 4.2f) return 100;
    if (voltage <= 3.0f) return 0;
    
    // 线性映射电压到百分比
    return (int)((voltage - 3.0f) / 1.2f * 100.0f);
}

void formatDataTask(void *pVoid);

void initFmtVars();

void handleSerialInput();

void handleCarrier();

void handlePreamble();

void revertFrequency();

#ifdef HAS_DISPLAY

void pword(const char *msg, int xloc, int yloc) {
    int dspW = u8g2->getDisplayWidth();
    int strW = 0;
    char glyph[2];
    glyph[1] = 0;
    for (const char *ptr = msg; *ptr; *ptr++) {
        glyph[0] = *ptr;
        strW += u8g2->getStrWidth(glyph);
        ++strW;
        if (xloc + strW > dspW) {
            int sxloc = xloc;
            while (msg < ptr) {
                glyph[0] = *msg++;
                xloc += u8g2->drawStr(xloc, yloc, glyph);
            }
            strW -= xloc - sxloc;
            yloc += u8g2->getMaxCharHeight();
            xloc = 0;
        }
    }
    while (*msg) {
        glyph[0] = *msg++;
        xloc += u8g2->drawStr(xloc, yloc, glyph);
    }
}

void showInitComp() {
    u8g2->clearBuffer();
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    // bottom (0,56,128,8)
    if (!no_wifi) {
        String ipa = WiFi.localIP().toString();
        u8g2->drawStr(0, 64, ipa.c_str());
    } else {
        u8g2->drawStr(0, 64, BLE_DEVICE_NAME);
    }
    if (have_sd && WiFiClass::status() == WL_CONNECTED)
        u8g2->drawStr(89, 64, "D");
    else if (have_sd)
        u8g2->drawStr(89, 64, "L");
    else if (WiFiClass::status() == WL_CONNECTED)
        u8g2->drawStr(89, 64, "N");
    char buffer[32];
    if (getLocalTime(&time_info, 0)) {
        sprintf(buffer, "%2u", ets_get_cpu_frequency() / 10);
        u8g2->drawStr(96, 64, buffer);
    } else {
        sprintf(buffer, "%uMHz", ets_get_cpu_frequency());
        int width = u8g2->getStrWidth(buffer);
        u8g2->drawStr(128 - width, 64, buffer);
    }

    float batt_voltage = battery.readVoltage() * 2;
    int batt_percent = voltageToPercentage(batt_voltage);
    
    if (getLocalTime(&time_info, 0)) {
        sprintf(buffer, "%d%%", batt_percent);
        u8g2->drawStr(108, 64, buffer);
    }
    // top (0,0,128,8)
    if (getLocalTime(&time_info, 0)) {
        sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
                time_info.tm_hour, time_info.tm_min);
        u8g2->drawStr(0, 7, buffer);
    } else {
        sprintf(buffer, "%d%%", batt_percent);
        u8g2->drawStr(0, 7, buffer);
    }
    u8g2->sendBuffer();
}

void updateInfo() {
    // update top
    char buffer[32];
    u8g2->setDrawColor(0);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    u8g2->drawBox(0, 0, 97, 8);
    u8g2->setDrawColor(1);
    if (getLocalTime(&time_info, 0)) {
        sprintf(buffer, "%d-%02d-%02d %02d:%02d", time_info.tm_year + 1900, time_info.tm_mon + 1, time_info.tm_mday,
                time_info.tm_hour, time_info.tm_min);
        u8g2->drawStr(0, 7, buffer);
    } else {
        int batt_percent = voltageToPercentage(voltage);
        sprintf(buffer, "%d%%", batt_percent);
        u8g2->drawStr(0, 7, buffer);
    }
#ifdef HAS_RTC
    sprintf(buffer, "%dC", (int) rtc.getTemperature());
    u8g2->drawStr(80, 7, buffer);
#endif
    // update bottom
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 56, 128, 8);
    u8g2->setDrawColor(1);
    if (!no_wifi) {
        String ipa = WiFi.localIP().toString();
        u8g2->drawStr(0, 64, ipa.c_str());
    } else
        u8g2->drawStr(0, 64, BLE_DEVICE_NAME);
    sprintf(buffer, "%.1f", getBias(actual_frequency));
    u8g2->drawStr(73, 64, buffer);
    if (sd1.status() && WiFiClass::status() == WL_CONNECTED)
        u8g2->drawStr(89, 64, "D");
    else if (sd1.status())
        u8g2->drawStr(89, 64, "L");
    else if (WiFiClass::status() == WL_CONNECTED)
        u8g2->drawStr(89, 64, "N");

    if (getLocalTime(&time_info, 0)) {
        sprintf(buffer, "%2u", ets_get_cpu_frequency() / 10);
        u8g2->drawStr(96, 64, buffer);
    } else {
        sprintf(buffer, "%uMHz", ets_get_cpu_frequency());
        int width = u8g2->getStrWidth(buffer);
        u8g2->drawStr(128 - width, 64, buffer);
    }
    voltage = battery.readVoltage() * 2;
    int batt_percent = voltageToPercentage(voltage);
    

    if (getLocalTime(&time_info, 0)) {
        sprintf(buffer, "%d%%", batt_percent);
        u8g2->drawStr(108, 64, buffer);
    }
    
    if (voltage < 3.15 && !low_volt_warned) {
        Serial.printf("Warning! Low Voltage detected, %1.2fV (%d%%)\n", voltage, batt_percent);
        sd1.append("低压警告，电池电压%1.2fV (%d%%)\n", voltage, batt_percent);
        low_volt_warned = true;
    }
    u8g2->sendBuffer();
}

void showSTR(const String &str) {
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    // u8g2->setFont(FONT_12_GB2312);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    pword(str.c_str(), 0, 19);
    u8g2->sendBuffer();
}

void showLBJ0(const struct lbj_data &l) {
    // box y 9->55
    char buffer[128];
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_wqy15_t_custom);
    u8g2->setCursor(0, 21);
    u8g2->printf("车  次");
    u8g2->setFont(u8g2_font_spleen8x16_mu);
    u8g2->setCursor(50, u8g2->getCursorY());
    u8g2->printf("%s", l.train);
    u8g2->setFont(u8g2_font_wqy15_t_custom);
    u8g2->setCursor(u8g2->getCursorX() + 6, u8g2->getCursorY());
    if (l.direction == FUNCTION_UP) {
        u8g2->printf("上行");
    } else if (l.direction == FUNCTION_DOWN)
        u8g2->printf("下行");
    else
        u8g2->printf("%d", l.direction);
    u8g2->setCursor(0, 37);
    u8g2->printf("速  度");
    u8g2->setCursor(50, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_spleen8x16_mu);
    u8g2->printf(" %s ", l.speed);
    u8g2->setCursor(u8g2->getCursorX() + 7, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_profont15_mr);
    u8g2->printf("KM/H");
    u8g2->setFont(u8g2_font_wqy15_t_custom);
    // sprintf(buffer, "公里标 %s KM", l.position);
    u8g2->setCursor(0, 53);
    u8g2->printf("公里标");
    u8g2->setCursor(50, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_spleen8x16_mu);
    u8g2->printf("%s ", l.position);
    u8g2->setCursor(u8g2->getCursorX() + 4, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_profont15_mr);
    u8g2->printf("KM");
    // draw RSSI
    u8g2->setDrawColor(0);
    u8g2->drawBox(98, 0, 30, 8);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", rxInfo.rssi);
    u8g2->drawStr(99, 7, buffer);
    u8g2->sendBuffer();
}

void showLBJ1(const struct lbj_data &l) {
    char buffer[128];
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(FONT_12_GB2312);
    // line 1
    u8g2->setCursor(0, 19);
    u8g2->printf("车:");
    u8g2->setCursor(u8g2->getCursorX() + 2, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_profont12_custom_tf);
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
    
    char cls_buf[3];
    int j = 0;
    for (int i = 0; i < 2 && l.lbj_class[i] != '\0'; i++) {
        if (l.lbj_class[i] != ' ') {
            cls_buf[j++] = l.lbj_class[i];
        }
    }
    cls_buf[j] = '\0';
    
    u8g2->printf("%s%s", cls_buf, buffer);
    u8g2->setFont(FONT_12_GB2312);
    u8g2->setCursor(68, 19);
    u8g2->printf("速:");
    u8g2->setCursor(u8g2->getCursorX() + 1, u8g2->getCursorY());
    u8g2->setFont(u8g2_font_profont12_custom_tf);
    u8g2->printf("%sKM/H", l.speed);
    u8g2->setFont(FONT_12_GB2312);
    // line 2
    u8g2->setCursor(0, 31);
    u8g2->printf("线:");
    u8g2->setCursor(u8g2->getCursorX() + 1, u8g2->getCursorY());
    u8g2->printf("%s", l.route_utf8);
    u8g2->drawBox(67, 21, 13, 12);
    u8g2->setDrawColor(0);
    if (l.direction == FUNCTION_UP)
        u8g2->drawUTF8(68, 31, "上");
    else if (l.direction == FUNCTION_DOWN)
        u8g2->drawUTF8(68, 31, "下");
    else {
        sprintf(buffer, "%d", l.direction);
        u8g2->drawStr(71, 31, buffer);
    }
    u8g2->setDrawColor(1);
    u8g2->setCursor(84, 31);
    u8g2->setFont(u8g2_font_profont12_custom_tf);
    u8g2->printf("%sK", l.position);
    u8g2->setFont(FONT_12_GB2312);
    // line 3
    u8g2->setCursor(0, 43);

    u8g2->setFont(FONT_12_GB2312);
    if (l.loco_type.length())
        u8g2->drawUTF8(0, 43, l.loco_type.c_str());
    
    u8g2->setFont(u8g2_font_profont12_custom_tf);
    
    char loco_with_suffix[16] = {0};
    if (String(l.loco) != "<NUL>" && l.info2_hex.length() > 14 && l.info2_hex[12] == '3') {
        if (l.info2_hex[13] == '1')
            sprintf(loco_with_suffix, "%sA", l.loco);
        else if (l.info2_hex[13] == '2')
            sprintf(loco_with_suffix, "%sB", l.loco);
        else
            strcpy(loco_with_suffix, l.loco);
    } else {
        strcpy(loco_with_suffix, l.loco);
    }
    
    if (strcmp(loco_with_suffix, "<NUL>") != 0) {
        int width = u8g2->getStrWidth(loco_with_suffix);
        u8g2->drawStr(128 - width, 43, loco_with_suffix);
    }
    char pos_buffer[64] = {0};
    
    if (l.pos_lat_deg[1] && l.pos_lat_min[1]) {
        sprintf(pos_buffer, "%s°%s'", l.pos_lat_deg, l.pos_lat_min);
    } else if (l.pos_lat[0] != '<') {
        sprintf(pos_buffer, "%s ", l.pos_lat);
    }
    
    if (l.pos_lon_deg[1] && l.pos_lon_min[1]) {
        sprintf(buffer, "%s°%s'", l.pos_lon_deg, l.pos_lon_min);
        strcat(pos_buffer, buffer);
    } else if (l.pos_lon[0] != '<') {
        sprintf(buffer, "%s", l.pos_lon);
        strcat(pos_buffer, buffer);
    }
    
    u8g2->setFont(u8g2_font_profont12_custom_tf);
    u8g2->drawUTF8(0, 54, pos_buffer);
    // draw RSSI
    u8g2->setDrawColor(0);
    u8g2->drawBox(98, 0, 30, 8);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", rxInfo.rssi);
    u8g2->drawStr(99, 7, buffer);
    u8g2->sendBuffer();
}

void showLBJ2(const struct lbj_data &l) {
    char buffer[128];
    u8g2->setDrawColor(0);
    u8g2->drawBox(0, 8, 128, 48);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_wqy15_t_custom);
    u8g2->setCursor(0, 23);
    u8g2->printf("当前时间");
    u8g2->setFont(u8g2_font_spleen8x16_mu);
    u8g2->setCursor(u8g2->getCursorX() + 3, u8g2->getCursorY() - 1);
    u8g2->printf("%s ", l.time);
    // draw RSSI
    u8g2->setDrawColor(0);
    u8g2->drawBox(98, 0, 30, 8);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_squeezed_b7_tr);
    sprintf(buffer, "%3.1f", rxInfo.rssi);
    u8g2->drawStr(99, 7, buffer);
    u8g2->sendBuffer();
}

#endif

void dualPrintf(bool time_stamp, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int len = vsnprintf(nullptr, 0, format, args);
    va_end(args);

    if (len < 0) {
        Serial.println("[ERROR] dualPrintf encoding error!");
        return;
    }

    std::vector<char> buffer(len + 1);
    va_start(args, format);
    vsnprintf(buffer.data(), buffer.size(), format, args);
    va_end(args);

    // Output to Serial
    Serial.print(buffer.data());
    if (telnet_online) { // code from Multimon-NG unixinput.c 还得是multimon-ng，chatGPT写了四五个版本都没解决。
        if (is_startline) {
            telnet.print("\r> ");
            if (time_stamp && getLocalTime(&time_info, 1))
                telnet.printf("\r%d-%02d-%02d %02d:%02d:%02d > ", time_info.tm_year + 1900, time_info.tm_mon + 1,
                              time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
            is_startline = false;
        }
        telnet.print(buffer.data());
        if (nullptr != strchr(buffer.data(), '\n')) {
            is_startline = true;
            telnet.print("\r< ");
        }
    }
}

void dualPrint(const char *fmt) {
    Serial.print(fmt);
    telnet.print(fmt);
}

void dualPrintln(const char *fmt) {
    Serial.println(fmt);
    telnet.println(fmt);
}

String printResetReason(esp_reset_reason_t reset) {
    String str;
    switch (reset) {
        case ESP_RST_UNKNOWN:
            str = "ESP_RST_UNKNOWN, Reset reason can not be determined";
            break;
        case ESP_RST_POWERON:
            str = "ESP_RST_POWERON, Reset due to power-on event";
            break;
        case ESP_RST_EXT:
            str = "ESP_RST_EXT, Reset by external pin (not applicable for ESP32)";
            break;
        case ESP_RST_SW:
            str = "ESP_RST_SW, Software reset via esp_restart";
            break;
        case ESP_RST_PANIC:
            str = "ESP_RST_PANIC, Software reset due to exception/panic";
            break;
        case ESP_RST_INT_WDT:
            str = "ESP_RST_INT_WDT, Reset (software or hardware) due to interrupt watchdog";
            break;
        case ESP_RST_TASK_WDT:
            str = "ESP_RST_TASK_WDT, Reset due to task watchdog";
            break;
        case ESP_RST_WDT:
            str = "ESP_RST_WDT, Reset due to other watchdogs";
            break;
        case ESP_RST_DEEPSLEEP:
            str = "ESP_RST_DEEPSLEEP, Reset after exiting deep sleep mode";
            break;
        case ESP_RST_BROWNOUT:
            str = "ESP_RST_BROWNOUT, Brownout reset (software or hardware)";
            break;
        case ESP_RST_SDIO:
            str = "ESP_RST_SDIO, Reset over SDIO";
            break;
    }
    return str;
}

void LBJTEST() {
    PagerClient::pocsag_data pocdat[16];
    pocdat[0].str = "37012";
    pocdat[0].addr = 1234000;
    pocdat[0].func = 1;
    pocdat[0].is_empty = false;
    pocdat[0].len = 15;
    pocdat[1].str = "30479100018530U)*9UU*6 (-(202011719040139058291000";
    pocdat[1].addr = 1234002;
    pocdat[1].func = 1;
    pocdat[1].is_empty = false;
    pocdat[1].len = 0;
//    Serial.println("[LBJ] 测试输出 机车编号 位置 XX°XX′XX″ ");
//    dualPrintf(false,"[LBJ] 测试输出 机车编号 位置 XX°XX′XX″ \n");
    struct lbj_data lbj;

    // db = new data_bond;
    // db->pocsagData[0].addr = 1234000;
    // db->pocsagData[0].str = "37012  15  1504";
    // db->pocsagData[0].func = 1;
    // db->pocsagData[0].is_empty = false;
    // db->pocsagData[0].len = 15;
    // db->pocsagData[1].str = "20202350018530U)*9UU*6 (-(202011719040139058291000";
    // db->pocsagData[1].addr = 1234002;
    // db->pocsagData[1].func = 1;
    // db->pocsagData[1].is_empty = false;
    // db->pocsagData[1].len = 0;
    readDataLBJ(pocdat, &lbj);
    printDataSerial(pocdat, lbj, rxInfo);
    // // appendDataLog(pocdat, lbj, rxInfo);
    // // printDataTelnet(pocdat, lbj, rxInfo);
    // rxInfo.rssi = 0;
    // rxInfo.fer = 0;
    // delete db;
}

int initPager() {// initialize SX1276 with default settings

    int state = radio.beginFSK(434.0, 4.8, 5.0, 12.5);
    RADIOLIB_ASSERT(state)

    state = radio.setGain(1);
    RADIOLIB_ASSERT(state)

    // state = radio.setRxBandwidth(25);
    // RADIOLIB_ASSERT(state)
    // initialize Pager client
    // Serial.print(F("[Pager] Initializing ... "));
    // base (center) frequency: 821.2375 MHz + ppm
    // speed:                   1200 bps
    state = pager.begin(actualFreq(ppm), 1200, false, 2500);
    RADIOLIB_ASSERT(state)

    freq_last = actual_frequency;

    // start receiving POCSAG messages
    // Serial.print(F("[Pager] Starting to listen ... "));
    // address of this "pager": 12340XX
    state = pager.startReceive(pin, 1234000, 0xFFFF0);
    //TODO Enhancement: try to keep a open address filter, we might find something unknown.
    RADIOLIB_ASSERT(state)

    // state = radio.setFrequency(actual_freq);
    // RADIOLIB_ASSERT(state)

    return (state);
}
//endregion

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) override {
        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            Serial.print("接收到数据: ");
            for (int i = 0; i < rxValue.length(); i++) {
                Serial.print(rxValue[i]);
            }
            Serial.println();
        }
    }
};

void initBLE() {
    Serial.println("[BLE] Initializing BLE...");
    
    if(BLEDevice::getInitialized()) {
        Serial.println("[BLE] Warning BLE already initialized, deinitializing first");
        BLEDevice::deinit(true);
        delay(500);
    }
    
    if (pServer != nullptr) {
        pServer = nullptr;
    }
    if (pTxCharacteristic != nullptr) {
        pTxCharacteristic = nullptr;
    }
    
    BLEDevice::init(BLE_DEVICE_NAME);
    Serial.println("[BLE] Initialization complete");
    
    pServer = BLEDevice::createServer();
    if (pServer == nullptr) {
        Serial.println("[BLE] Error unable to create server");
        return;
    }
    
    pServer->setCallbacks(new SafeServerCallbacks());
    Serial.println("[BLE] Server creation complete");
    
    BLEService *pService = pServer->createService(BLEUUID(SERVICE_UUID));
    Serial.println("[BLE] Service creation complete");
    
    pTxCharacteristic = pService->createCharacteristic(
                          CHARACTERISTIC_UUID_TX,
                          BLECharacteristic::PROPERTY_READ |
                          BLECharacteristic::PROPERTY_WRITE |
                          BLECharacteristic::PROPERTY_NOTIFY
                        );
    
    if (pTxCharacteristic == nullptr) {
        Serial.println("[BLE] Error unable to create TX characteristic");
        return;
    }
    
    BLE2902* p2902Descriptor = new BLE2902();
    if (p2902Descriptor != nullptr) {
        p2902Descriptor->setNotifications(true);
        pTxCharacteristic->addDescriptor(p2902Descriptor);
    } else {
        Serial.println("[BLE] Warning unable to create BLE2902 descriptor");
    }
    
    try {
        pTxCharacteristic->setValue("LBJ Ready");
        Serial.println("[BLE] TX characteristic value set to initial message");
    } catch (...) {
        Serial.println("[BLE] Error setting TX characteristic value");
    }
    
    pService->start();
    Serial.println("[BLE] Service started");
    
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    
    pAdvertising->start();
    Serial.println("[BLE] Advertising started");
    Serial.printf("[BLE] Device name: %s\n", BLE_DEVICE_NAME);
    Serial.printf("[BLE] Service UUID: %s\n", SERVICE_UUID);
    Serial.printf("[BLE] TX characteristic UUID: %s\n", CHARACTERISTIC_UUID_TX);
    Serial.println("[BLE] Server ready awaiting client connection");
}

void sendTrainDataOverBLE(const struct lbj_data &l, const struct rx_info &r, bool isTest) {
    if (!ble_enabled) {
        Serial.println("[BLE] Warning trying to send data while BLE is disabled");
        return;
    }

    if (pTxCharacteristic == nullptr) {
        Serial.println("[BLE] Error TX characteristic is null cannot send data");
        return;
    }

    if (!deviceConnected.load(std::memory_order_acquire)) {
        Serial.println("[BLE] Info no device connected not sending data");
        return;
    }

    try {
        DynamicJsonDocument doc(1024); // Allocate a JSON document

        doc["train"] = l.train;
        doc["dir"] = l.direction;
        doc["speed"] = l.speed;
        doc["pos"] = l.position;
        doc["time"] = l.time;
        doc["loco"] = l.loco;
        doc["loco_type"] = l.loco_type;
        doc["lbj_class"] = l.lbj_class;
        doc["route"] = l.route_utf8;

        String position_info = "";
        if (l.pos_lat_deg[0] && l.pos_lat_min[0] && l.pos_lon_deg[0] && l.pos_lon_min[0]) {
            position_info = String(l.pos_lat_deg) + "°" + String(l.pos_lat_min) + "′ "
                          + String(l.pos_lon_deg) + "°" + String(l.pos_lon_min) + "′";
        } else if (l.pos_lat[0] && l.pos_lon[0]) {
            position_info = String(l.pos_lat) + " " + String(l.pos_lon);
        }
        doc["position_info"] = position_info;
        doc["rssi"] = r.rssi;
        doc["test_flag"] = isTest;

        size_t jsonSize = measureJson(doc);
        
        if (jsonSize > 65536) { 
            Serial.printf("[BLE][ERROR] JSON unreasonably large: %zu bytes (max 65536)\n", jsonSize);
            return;
        }
        
        String output;
        output.reserve(jsonSize + 10);
        serializeJson(doc, output);
        
        if (deviceConnected.load(std::memory_order_acquire) && pTxCharacteristic != nullptr) {
            if (jsonSize > 1024) {
                Serial.printf("[BLE] Large JSON packet: %zu bytes\n", jsonSize);
            }
            
            pTxCharacteristic->setValue(output.c_str());
            pTxCharacteristic->notify();
            Serial.println("[BLE] Data sent");
        }
    } catch (const std::exception& e) {
        Serial.printf("[BLE] Data send exception: %s\n", e.what());
    } catch (...) {
        Serial.println("[BLE] Unknown exception occurred while sending data");
    }
}
void readCsvAndSendBLE() {}

bool sendNextHistoryDataOverBLE() {
    return false;
}

// SETUP
void setup() {
    esp_core_dump_init();
    runtime_timer = millis64();
    esp_reset_reason_t reset_reason = esp_reset_reason();
    initBoard();
    sd1.setFS(SD);
    delay(150);

    data_mutex = xSemaphoreCreateMutex();
    if (data_mutex == nullptr) {
        Serial.println("[ERROR] Failed to create data mutex!");
        while(true);
    }

    xQueue_db = xQueueCreate(5, sizeof(data_bond*));
    if (xQueue_db == nullptr) {
        Serial.println("[ERROR] Failed to create data queue!");
        while(true);
    }

    // Configure time sync.
    sntp_set_time_sync_notification_cb(timeAvailable);
    sntp_servermode_dhcp(1);
    configTzTime(time_zone, ntpServer1, ntpServer2);

#ifdef HAS_RTC
    // rtc.begin();
    // rtc.getDateTime(time_info);
    time_info = rtcLibtoC(rtc.now());
    Serial.println(&time_info, "[eRTC] RTC Time %Y-%m-%d %H:%M:%S ");
    timeSync(time_info); // sync system time from rtc
    Serial.printf("SYS Time %s\n", fmtime(time_info));
#endif

    Serial.printf("RST: %s\n", printResetReason(reset_reason).c_str());
    if (have_sd) {
        sd1.begin("/LOGTEST");
        sd1.beginCSV("/CSVTEST");
        sd1.append("电池电压 %1.2fV\n", battery.readVoltage() * 2);
        sd1.append(2, "调试等级 %d\n", LOG_VERBOSITY);
        sd1.append("复位原因 %s\n", printResetReason(reset_reason).c_str());
#ifdef HAS_RTC
        sd1.append("RTC时间 %d-%02d-%02d %02d:%02d:%02d\n", time_info.tm_year + 1900, time_info.tm_mon + 1,
                   time_info.tm_mday, time_info.tm_hour, time_info.tm_min, time_info.tm_sec);
#endif
    }

    // Process core dump.
    readCoreDump();

    if (u8g2) {
        showInitComp();
        u8g2->setFont(FONT_12_GB2312);
        u8g2->setCursor(0, 52);
        u8g2->println("Initializing...");
        u8g2->sendBuffer();
    }

#ifdef USE_SMARTCONFIG
    // initialize wireless network.
    Serial.printf("Connecting to WiFi\n");

    Preferences preferences;
    preferences.begin("wifi-config", false);

    String savedSSID = preferences.getString("ssid", "");
    String savedPassword = preferences.getString("password", "");

    if (!savedSSID.isEmpty() && !savedPassword.isEmpty()) {
        if (u8g2) {
            u8g2->setDrawColor(0);
            u8g2->drawBox(0, 42, 128, 14);
            u8g2->setDrawColor(1);
            u8g2->setCursor(0, 52);
            u8g2->println("Waiting for WiFi...");
            u8g2->sendBuffer();
        }
        if (!connectToWiFi(savedSSID, savedPassword, 10000)) {
            if (u8g2) {
                u8g2->setDrawColor(0);
                u8g2->drawBox(0, 42, 128, 14);
                u8g2->setDrawColor(1);
                u8g2->setCursor(0, 40);
                u8g2->println("Failed to connect to Wifi");
                u8g2->setCursor(0, 52);
                u8g2->println("Waiting for SmartConfig...");
                u8g2->sendBuffer();
            }
            performSmartConfig();
        }
    } else {
        if (u8g2) {
            u8g2->setDrawColor(0);
            u8g2->drawBox(0, 42, 128, 14);
            u8g2->setDrawColor(1);
            u8g2->setCursor(0, 52);
            u8g2->println("Waiting for SmartConfig...");
            u8g2->sendBuffer();
        }
        performSmartConfig();
    }

    Serial.print("[Network]IP Address: ");
    Serial.println(WiFi.localIP());
    WiFi.setAutoReconnect(true);
    WiFi.persistent(true);
    preferences.putString("ssid", WiFi.SSID());
    preferences.putString("password", WiFi.psk());
    preferences.end();
    wifiSSID = WiFi.SSID();
    wifiPassword = WiFi.psk();
#else
    // initialize wireless network.
    Serial.printf("Connecting to WiFi %s\n", wifiSSID.c_str());
    if (u8g2) {
        u8g2->setDrawColor(0);
        u8g2->drawBox(0, 42, 128, 14);
        u8g2->setDrawColor(1);
        u8g2->drawStr(0, 52, "Connecting to WiFi...");
        u8g2->sendBuffer();
    }
    connectToWiFi(wifiSSID, wifiPassword, 1000);
#endif

    if (isConnected()) {
        ip = WiFi.localIP();
        // Serial.println();
        Serial.print("[Telnet] ");
        Serial.print(ip);
        Serial.print(":");
        Serial.println(port);
        setupTelnet(); // todo: find another library / modify the code to support multiple client connection.
    } else {
        // Serial.println();
        Serial.println("Error connecting to WiFi, Telnet startup skipped.");
    }

    // Initialize SX1276
    dualPrint("[SX1276] Initializing ... ");
    int state = initPager();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println(F("success."));
        Serial.printf("[SX1276] Actual Frequency %f MHz, ppm %.1f\n", actualFreq(ppm), ppm);
    } else {
        Serial.print(F("failed, code "));
        Serial.println(state);
        while (true);
    }

//    if(WiFi.getSleep())
//        Serial.println("WIFI Sleep enabled.");

    // start thread watchdog
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(nullptr);
    // wdt_timer = millis64();

    digitalWrite(BOARD_LED, LED_OFF);
    Serial.printf("Booting time %llu ms\n", millis64() - runtime_timer);
    sd1.append("启动用时 %llu ms\n", millis64() - runtime_timer);

    runtime_timer = 0;

    if (u8g2) {
        u8g2->setDrawColor(0);
        u8g2->drawBox(0, 42, 128, 14);
        u8g2->setDrawColor(1);
        u8g2->drawStr(0, 52, "Listening...");
        u8g2->sendBuffer();
        Serial.printf("Mem left: %d Bytes\n", esp_get_free_heap_size());
    }

    // test stuff
    // LBJTEST();
    // auto *test = new uint8_t[32];
    // Serial.printf("[D] test addr %p\n",test);
    // delete[] test;
    // Serial.printf("[D] test addr %p\n",test);
    // test = nullptr;
    // Serial.printf("[D] test addr %p\n",test);
    // delete[] test;
    // delete[] test;
//     Serial.printf("CPU FREQ %d MHz\n",ets_get_cpu_frequency());
    if (ble_enabled) {
        initBLE();
    }

    xTaskCreatePinnedToCore(formatDataTask, "task_fd", FD_TASK_STACK_SIZE, nullptr, 2, nullptr, ARDUINO_RUNNING_CORE);
}

// Loop functions
void handleTelnetCall() {
    if (give_tel_rssi) {
        telnet.printf("> RSSI %3.2f dBm.\n", radio.getRSSI(false, true));
        give_tel_rssi = false;
        telnet.print("< ");
    }
    if (give_tel_gain) {
        telnet.printf("> Gain Pos %d \n", radio.getGain());
        give_tel_gain = false;
        telnet.print("< ");
    }
    if (tel_set_ppm) {
        int16_t state = radio.setFrequency(actualFreq(ppm));
        if (state == RADIOLIB_ERR_NONE) {
            telnet.printf("> Actual Frequency %f MHz\n", actualFreq(ppm));
            Serial.printf("[Telnet] > Actual Frequency %f MHz\n", actualFreq(ppm));
        } else {
            telnet.printf("> Failure, Code %d\n", state);
            Serial.printf("[Telnet] > Failure, Code %d\n", state);
        }
        telnet.printf("> ppm set to %.f\n", ppm);
        tel_set_ppm = false;
        telnet.print("< ");
    }
}

void handleSync() {
    if (pager.gotSyncState()) {
        // if (!bandwidth_altered) {
        //     int16_t state = radio.swapRxBandwidth(12.5);
        //     Serial.printf("[D] Channelize, code %d\n",state);
        //     radio.restartReceive(true);
        //     bandwidth_altered = true;
        // }
//        sd1.append("[PGR][DEBUG] SYNC DETECTED.\n");
        if (rxInfo.cnt < 5 && (rxInfo.timer == 0 || esp_timer_get_time() - rxInfo.timer < 11000)) {
            float rssi = radio.getRSSI(false, true);
            rxInfo.timer = esp_timer_get_time();
            // rxInfo.rssi += rssi;
            rssi_cache += rssi;
            rxInfo.cnt++;
            Serial.printf("[D] RXI %.2f\n", rssi_cache / (float) rxInfo.cnt);
        }
        if (rxInfo.fer == 0)
            rxInfo.fer = radio.getFrequencyError();
    }
}


void handleTelnet() {
    if (isConnected() && !telnet_online) {
        ip = WiFi.localIP();
        Serial.printf("WIFI Connection to %s established.\n", wifiSSID.c_str());
        Serial.print("[Telnet] ");
        Serial.print(ip);
        Serial.print(":");
        Serial.println(port);
        setupTelnet();
    }
    telnet.loop();
}

void checkNetwork() {
    if (isConnected() && net_timer != 0)
        net_timer = 0;
    else if (!isConnected() && net_timer == 0)
        net_timer = millis64();

    if (!isConnected() && millis64() - net_timer > NETWORK_TIMEOUT && !no_wifi) { // 暂定解决方案：超30分钟断wifi
        telnet.stop();
        telnet_online = false;
        WiFi.disconnect();
        WiFiClass::mode(WIFI_OFF);
        Serial.println("WIFI off after 30 minutes without connection.");
        no_wifi = true;
    }

    if (ip_last != WiFi.localIP()) {
        Serial.print("Local IP ");
        Serial.print(WiFi.localIP());
        Serial.print("\n");
    }
    ip_last = WiFi.localIP();
}

void handleBleConnections() {
    if (pServer == nullptr) {
        return;
    }
    
    if (!deviceConnected.load(std::memory_order_acquire) && oldDeviceConnected) {
        Serial.println("[BLE] Device disconnected, reinitializing BLE service");
        
        try {
            if (pServer != nullptr) {
                BLEAdvertising *pAdvertising = pServer->getAdvertising();
                if (pAdvertising != nullptr) {
                    pAdvertising->stop();
                    Serial.println("[BLE] Stopped advertising");
                }
                
                delay(100);
                
                if (pTxCharacteristic != nullptr) {
                    pTxCharacteristic->setValue("");
                }
                
                pAdvertising = pServer->getAdvertising();
                pAdvertising->addServiceUUID(SERVICE_UUID);
                pAdvertising->setScanResponse(true);
                pAdvertising->setMinPreferred(0x06);
                pAdvertising->setMaxPreferred(0x12);
                pAdvertising->setMinInterval(0x20);
                pAdvertising->setMaxInterval(0x40);
                
                pAdvertising->start();
                Serial.println("[BLE] Restarted advertising after disconnect");
            }
        } catch (const std::exception& e) {
            Serial.printf("[BLE] Exception during reconnection: %s\n", e.what());
        } catch (...) {
            Serial.println("[BLE] Unknown error during reconnection");
        }
        
        oldDeviceConnected = deviceConnected.load(std::memory_order_acquire);
    }
    
    if (deviceConnected.load(std::memory_order_acquire) && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected.load(std::memory_order_acquire);
        if (pTxCharacteristic != nullptr) {
            pTxCharacteristic->setValue("LBJ Train Warning Ready");
            pTxCharacteristic->notify();
        }
        Serial.println("[BLE] New device connected successfully");
    }
}

// LOOP
void loop() {
    // reset watchdog
    esp_task_wdt_reset();
    // if (millis64() - wdt_timer >= WDT_RST_PERIOD) {
    //     uint64_t t = esp_timer_get_time() ;
    //     auto r = esp_task_wdt_reset();
    //     t = esp_timer_get_time() - t;
    //     wdt_timer = millis64();
    //     Serial.printf("WDT_RST %d [%llu]\n",r,t);
    // }

    // freqCorrection();
    // Handle carrier timout.
    if (car_timer != 0 && millis64() - car_timer > 700 && prb_timer == 0 && rxInfo.timer == 0) {
        car_count = 0;
        revertFrequency();
        car_fer_last = 0;
        car_timer = 0;
        Serial.println("[D] CARRIER TIMEOUT.");
    }

    // Handle preamble timeout.
    if (prb_timer != 0 && millis64() - prb_timer > 600 && rxInfo.timer == 0) {
        prb_count = 0;
        revertFrequency();
        for (auto &i: fers) {
            i = 0;
        }
        prb_timer = 0;
        Serial.println("[D] PREAMBLE TIMEOUT.");
    }

    // if task complete, de-initialize
    // The formatDataTask is now a permanent task, so no need to de-initialize or handle timeouts here.
    // Data is passed via queue.

    if (millis64() - led_timer > LED_ON_TIME && led_timer != 0) {
        digitalWrite(BOARD_LED, LED_OFF);
        led_timer = 0;
        changeCpuFreq(240);
    }

    handleSerialInput();
    checkNetwork();
    handleTelnet();
    handleTelnetCall();
    
    if (ble_enabled) {
        handleBleConnections();
        
        if (deviceConnected.load(std::memory_order_acquire)) {
            if (ble_timer == 0 || millis64() - ble_timer > 30000) {
                Serial.println("[BLE] Connection active waiting for actual data");
                ble_timer = millis64();
            }
        }
    }

    if (millis64() > 60000 && !exec_init_f80) // lower down frequency 60 sec after startup and idle.
    {
        if (isConnected())
            setCpuFrequencyMhz(80);
        else {
            WiFiClass::mode(WIFI_OFF);
            setCpuFrequencyMhz(80);
            WiFiClass::mode(WIFI_MODE_STA);
#ifdef USE_SMARTCONFIG
            // connectWiFi();
            WiFi.begin(wifiSSID, wifiPassword);
#else
            WiFi.begin(wifiSSID, wifiPassword);
#endif
        }
        exec_init_f80 = true;
    }
#ifdef HAS_DISPLAY
    // update information on screen.
    if (screen_timer == 0) {
        screen_timer = millis64();
    } else if (millis64() - screen_timer > 3000) { // Set to 3000 to reduce interference.
#ifdef HAS_OLED_TIMEOUT
        if (!oled_off)
#endif
            updateInfo();
        screen_timer = millis64();
    }
#ifdef HAS_OLED_TIMEOUT
    if (millis64() - timer4 >= OLED_TIMEOUT && timer4 != 0 && !oled_off) {
        u8g2->clearBuffer();
        oled_off = true;
        u8g2->setPowerSave(true);
    }
#endif
#endif

    if (millis64() - timer4 >= 60000 && timer4 != 0 && ets_get_cpu_frequency() != 80) // fCPU to 80 after 60s in idle.
        changeCpuFreq(80);

    handleCarrier();
    handlePreamble();

    handleSync();

    // the number of batches to wait for
    // 2 batches will usually be enough to fit short and medium messages
    if (pager.available() >= 2) {
        setCpuFrequencyMhz(240);
        db = new data_bond;
        runtime_timer = millis64();
        timer4 = millis64();
        int state = pager.readDataMSA(db->pocsagData, 0);
        rxInfo.rssi = rssi_cache / (float) rxInfo.cnt;
        rssi_cache = 0;
        rxInfo.cnt = 0;
        rxInfo.timer = 0;
        prb_timer = 0;
        car_timer = 0;

        Serial.printf("[D] Prb_count %d\n", prb_count);
        Serial.printf("[D] Car_count %d\n", car_count);
        if (prb_count >= 32)
            prb_count = 31;
        if (prb_count > 0)
            rxInfo.fer = fers[prb_count - 1];
        for (int i = 0; i < prb_count; ++i) {
            Serial.printf("[D] Fer %.2f Hz\n", fers[i]);
            fers[i] = 0;
        }
        prb_count = 0;
        car_count = 0;
        car_fer_last = 0;
        rxInfo.ppm = getBias(actual_frequency);

        Serial.println(F("[Pager] Received pager data, decoding ... "));
        sd1.append(2, "正在解码信号...\n");

        if (state == RADIOLIB_ERR_NONE) {
            freq_last = actual_frequency;
            digitalWrite(BOARD_LED, LED_ON);
            led_timer = millis64();

            sd1.append(2, "正在格式化输出...\n");
            if (xQueueSend(xQueue_db, &db, portMAX_DELAY) != pdPASS) {
                Serial.println("[ERROR] Failed to send data to queue!");
                sd1.append("[ERROR] Failed to send data to queue!\n");
                delete db;
                db = nullptr;
                initFmtVars();
            }
        } else if (state == RADIOLIB_ERR_MSG_CORRUPT) {
            dualPrintf(true, "[Pager] Reception failed, too many errors. \n");
            revertFrequency();
            delete db;
            db = nullptr;
            initFmtVars();
        } else {
            sd1.append("[Pager] Reception failed, code %d \n", state);
            dualPrintf(true, "[Pager] Reception failed, code %d \n", state);
            delete db;
            db = nullptr;
            initFmtVars();
        }
    }
}

void revertFrequency() {
    if (actual_frequency != freq_last) {
        actual_frequency = freq_last;
        int state = radio.setFrequency(actual_frequency);
        if (state != RADIOLIB_ERR_NONE) {
            Serial.printf("[D] Revert freq failed %d\n", state);
        } else {
            Serial.printf("[D] Revert to last freq %f MHz, ppm %.2f\n", actual_frequency, getBias(actual_frequency));
        }
    }
}

void handleCarrier() {
    if (pager.gotCarrierState() && !pager.gotPreambleState() && !pager.gotSyncState() && freq_correction &&
        prb_timer == 0) {
        if (car_count == 0)
            car_timer = millis64();
        ++car_count;
        if (car_count < 64) {
            float fei = radio.getFrequencyError();
            // Serial.printf("[D] Carrier FEI %.2f Hz, count %d\n",fei,car_count);
            if (abs(fei) > 1000.0 && car_count != 1 &&
                abs(fei - car_fer_last) < 500) {
                // Perform frequency correction
                auto target_freq = (float) (actual_frequency + fei * 1e-6);
                int state = radio.setFrequency(target_freq);
                if (state != RADIOLIB_ERR_NONE) {
                    Serial.printf("[D][C] Freq Alter failed %d, target freq %f\n", state, target_freq);
                    sd1.append("[D][C] Freq Alter failed %d, target freq %f\n", state, target_freq);
                } else {
                    actual_frequency = target_freq;
                    Serial.printf("[D][C] Freq Altered %f MHz, FEI %.2f Hz, PPM %.2f\n", actual_frequency, fei,
                                  getBias(actual_frequency));
                }
            }
            car_fer_last = fei;
        }
    }
}

void handlePreamble() {
    if (pager.gotPreambleState() && !pager.gotSyncState() && freq_correction) {
        if (prb_count == 0)
            prb_timer = millis64();
        // if (millis64() - prb_timer > 500) {
        //     prb_count = 0;
        //     if (actual_frequency != freq_last) {
        //         actual_frequency = freq_last;
        //         int state = radio.setFrequency(actual_frequency);
        //         if (state != RADIOLIB_ERR_NONE) {
        //             Serial.printf("[D] Freq Alter failed %d\n", state);
        //         }
        //     }
        //     for (auto &i: fers) {
        //         i = 0;
        //     }
        // }
        ++prb_count;
        if (prb_count < 32) {
            // todo: Implement automatic bandwidth adjustment.
            // if (prb_count > 2 && !bandwidth_altered) {
            //     int16_t state = radio.swapRxBandwidth(12.5);
            //     Serial.printf("[D] Bandwidth to 12.5,code %d\n",state);
            //     radio.restartReceive(true);
            //     bandwidth_altered = true;
            // }
            // else if (prb_count > 6 && bandwidth_altered) {
            //     radio.swapRxBandwidth(12.5);
            //     bandwidth_altered = false;
            // }
            // radio.swapRxBandwidth(12.5);
            fers[prb_count - 1] = radio.getFrequencyError();
            if (abs(fers[prb_count - 1]) > 1000.0 && prb_count != 1 &&
                abs(fers[prb_count - 1] - fers[prb_count - 2]) < 500) {
                // Perform frequency correction
                auto target_freq = (float) (actual_frequency + fers[prb_count - 1] * 1e-6);
                int state = radio.setFrequency(target_freq);
                if (state != RADIOLIB_ERR_NONE) {
                    Serial.printf("[D][P] Freq Alter failed %d, target freq %f\n", state, target_freq);
                    sd1.append("[D][P] Freq Alter failed %d, target freq %f\n", state, target_freq);
                } else {
                    actual_frequency = target_freq;
                    Serial.printf("[D][P] Freq Altered %f MHz, FEI %.2f Hz, PPM %.2f\n", actual_frequency,
                                  fers[prb_count - 1], getBias(actual_frequency));
                }
            }
        }
    }
}

void getCoreFreq(void *pVoid) {
    Serial.printf("Core %d Frequency %d MHz\n", xPortGetCoreID(), ets_get_cpu_frequency());
    vTaskDelete(nullptr);
}

void handleSerialInput() {
    if (Serial.available()) {
        String in = Serial.readStringUntil('\r');
        
        if (in == "ble on") {
            if (!ble_enabled) {
                ble_enabled = true;
                initBLE();
                Serial.println("[BLE] Enabled");
            } else {
                BLEDevice::deinit(true);
                delay(500);
                initBLE();
                Serial.println("[BLE] Reinitialized");
            }
        }
        else if (in == "ble off") {
            if (ble_enabled) {
                ble_enabled = false;
                BLEDevice::deinit(true);
                Serial.println("[BLE] Disabled");
            } else {
                Serial.println("[BLE] Already disabled");
            }
        }
        else if (in == "ble status") {
            Serial.printf("[BLE] Status: %s, Connection: %s\n",
                ble_enabled ? "enabled" : "disabled",
                deviceConnected.load(std::memory_order_acquire) ? "Connected" : "Disconnected");
        }
        else if (in == "ping")
            Serial.println("$ Pong");
        else if (in == "rtc") {
#ifdef HAS_RTC
            // rtc.getDateTime(time_info);
            // DateTime now = rtc.now();
            time_info = rtcLibtoC(rtc.now());
            float temp = rtc.getTemperature();
            Serial.print(&time_info, "$ [eRTC] %Y-%m-%d %H:%M:%S ");
            Serial.printf("Temp: %.2f °C\n", temp);
#endif
        } else if (in == "time") {
            getLocalTime(&time_info, 1);
            Serial.printf("$ SYS Time %s, Up time %llu ms (%s)\n", fmtime(time_info), millis64(), fmtms(millis64()));
        } else if (in == "cd") {
            if (have_cd)
                Serial.println("$ Core dump exported.");
            else
                Serial.println("$ No core dump.");
        } else if (in == "sd end") {
            if (!sd1.status())
                Serial.println("$ [SDLOG] No SD.");
            else {
                sd1.append("[SDLOG] SD卡将被卸载\n");
                sd1.end();
                Serial.println("$ [SDLOG] SD end.");
            }
        } else if (in == "sd begin") {
            if (sd1.status())
                Serial.println("$ End SD First.");
            else {
                SD_LOG::reopenSD();
                sd1.begin("/LOGTEST");
                sd1.beginCSV("/CSVTEST");
                sd1.append("[SDLOG] SD卡已重新挂载\n");
                Serial.println("$ [SDLOG] SD reopen.");
            }
        } else if (in == "mem") {
            Serial.printf("$ Mem left: %d Bytes\n", esp_get_free_heap_size());
        } else if (in == "rst") {
            esp_reset_reason_t reason = esp_reset_reason();
            Serial.printf("$ RST: %s\n", printResetReason(reason).c_str());
        } else if (in == "ppm") {
            if (runtime_timer == 0 && !pager.gotSyncState()) {
                ppm = 3;
                int16_t state = radio.setFrequency(actualFreq(ppm));
                if (state == RADIOLIB_ERR_NONE)
                    Serial.printf("$ Actual Frequency %f MHz\n", actualFreq(ppm));
                else
                    Serial.printf("$ Failure, Code %d\n", state);
            } else {
                Serial.println("$ Unable to change frequency due to occupation");
                if (pager.available())
                    Serial.println("$ pager.available == true");
                if (runtime_timer)
                    Serial.printf("$ runtime_timer = %llu, running %llu\n", runtime_timer, millis64() - runtime_timer);
            }
        } else if (in == "ppm read") {
            Serial.printf("$ ppm %.1f\n", ppm);
        } else if (in == "afc off") {
            prb_count = 0;
            prb_timer = 0;
            car_count = 0;
            car_timer = 0;
            freq_correction = false;
            Serial.println("$ Frequency Correction Disabled");
        } else if (in == "afc on") {
            freq_correction = true;
            Serial.println("$ Frequency Correction Enabled");
        } else if (in == "rssi") {
            Serial.printf("$ RSSI %3.2f dBm.\n", radio.getRSSI(false, true));
        } else if (in == "gain") {
            Serial.printf("$ Gain Pos %d \n", radio.getGain());
        } else if (in == "cpu") {
            xTaskCreatePinnedToCore(getCoreFreq, "get_freq", 2048, nullptr,
                                    1, nullptr, 0);
            Serial.printf("Core %d Frequency %d MHz\n", xPortGetCoreID(), ets_get_cpu_frequency());
        }
    }
}

void initFmtVars() {
    Serial.printf("[Pager] Processing time %llu ms.\n", millis64() - runtime_timer);
    runtime_timer = 0;
    rxInfo.rssi = 0;
    rxInfo.fer = 0;
    rxInfo.ppm = 0;
    // prb_count = 0;
    // for (auto &i: fers) {
    //     i = 0;
    // }
    if (db != nullptr) {
        delete db;
        db = nullptr;
    }
}

void formatDataTask(void *pVoid) {
    data_bond *db_local = nullptr;
    for (;;) {
        if (xQueueReceive(xQueue_db, &db_local, portMAX_DELAY) == pdTRUE) {
            // Serial.printf("[FD-Task] Stack High Mark Begin %u\n", uxTaskGetStackHighWaterMark(nullptr));
            sd1.append(2, "格式化任务已创建\n");
            
            for (auto &i: db_local->pocsagData) {
                if (i.is_empty)
                    continue;
                Serial.printf("[D-pDATA] %d/%d: %s\n", i.addr, i.func, i.str.c_str());
                sd1.append(2, "[D-pDATA] %d/%d: %s\n", i.addr, i.func, i.str.c_str());
                
                db_local->str = db_local->str + "  " + i.str;
            }

            // Serial.printf("[FD-Task] Stack High Mark pDATA %u\n", uxTaskGetStackHighWaterMark(nullptr));
            sd1.append(2, "原始数据输出完成，用时[%llu]\n", millis64() - runtime_timer);
            Serial.printf("decode complete.[%llu]", millis64() - runtime_timer);
            readDataLBJ(db_local->pocsagData, &db_local->lbjData);
            sd1.append(2, "LBJ读取完成，用时[%llu]\n", millis64() - runtime_timer);
            Serial.printf("Read complete.[%llu]", millis64() - runtime_timer);
            // Serial.printf("[FD-Task] Stack High Mark rLBJ %u\n", uxTaskGetStackHighWaterMark(nullptr));

            printDataSerial(db_local->pocsagData, db_local->lbjData, rxInfo);
            sd1.append(2, "串口输出完成，用时[%llu]\n", millis64() - runtime_timer);
            
            if (data_mutex != nullptr) {
                if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                    current_lbj_data = db_local->lbjData;
                    xSemaphoreGive(data_mutex);
                } else {
                    Serial.println("[ERROR] data_mutex take timeout in formatDataTask - CRITICAL!");
                }
            } else {
                Serial.println("[ERROR] data_mutex is null in formatDataTask - CRITICAL!");
            }
            
            if (ble_enabled && db_local != nullptr) {
                try {
                    sendTrainDataOverBLE(db_local->lbjData, rxInfo);
                } catch (...) {
                    Serial.println("[BLE] Error occurred while sending data over BLE");
                }
            }
            Serial.printf("SPRINT complete.[%llu]", millis64() - runtime_timer);

            // sd1.disableSizeCheck();
            appendDataLog(db_local->pocsagData, db_local->lbjData, rxInfo);
            Serial.printf("sdprint complete.[%llu]", millis64() - runtime_timer);
            appendDataCSV(db_local->pocsagData, db_local->lbjData, rxInfo);
            Serial.printf("csvprint complete.[%llu]", millis64() - runtime_timer);
            
            // sd1.enableSizeCheck();

            printDataTelnet(db_local->pocsagData, db_local->lbjData, rxInfo);
            Serial.printf("telprint complete.[%llu]", millis64() - runtime_timer);
            // Serial.printf("[FD-Task] Stack High Mark TRI-OUT %u\n", uxTaskGetStackHighWaterMark(nullptr));
// Serial.printf("type %d \n",lbj.type);

#ifdef HAS_DISPLAY
            // fd_state = TASK_RUNNING_SCREEN;
            if (u8g2) {
#ifdef HAS_OLED_TIMEOUT
                if (oled_off) {
                    oled_off = false;
                    u8g2->setPowerSave(false);
                    u8g2->clearBuffer();
                    updateInfo();
                }
#endif
                if (db_local->lbjData.type == 0)
                    showLBJ0(db_local->lbjData);
                else if (db_local->lbjData.type == 1) {
                    showLBJ1(db_local->lbjData);
                } else if (db_local->lbjData.type == 2) {
                    showLBJ2(db_local->lbjData);
                }
                Serial.printf("Complete u8g2 [%llu]\n", millis64() - runtime_timer);
            }
#endif
            Serial.printf("[FD-Task] Stack High Mark %u\n", uxTaskGetStackHighWaterMark(nullptr));
            sd1.append(2, "任务堆栈标 %u\n", uxTaskGetStackHighWaterMark(nullptr));
            // sd1.append("[FD-Task] Stack High Mark %u\n", uxTaskGetStackHighWaterMark(nullptr));
            sd1.append(2, "格式化输出任务完成，用时[%llu]\n", millis64() - runtime_timer);
            
            if (db_local != nullptr) {
                delete db_local;
                db_local = nullptr;
            }
            // fd_state = TASK_DONE;
            // task_fd = nullptr;
            // vTaskDelete(nullptr);
        }
    }
}

// END OF FILE.