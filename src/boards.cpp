//
// Created by FLN1021 on 2023/9/10.
//

#include "boards.hpp"
#include <memory>

#ifdef HAS_DISPLAY
std::unique_ptr<DISPLAY_MODEL> u8g2 = nullptr;
#endif

ESP32AnalogRead battery;
float voltage;
SPIClass SDSPI(HSPI);
bool have_sd = false;
bool have_rtc = false;

#ifdef HAS_RTC
RTC_DS3231 rtc;
#endif

uint64_t millis64() {
    return esp_timer_get_time() / 1000ULL;
}

void initBoard() {
    Serial.begin(115200);
    Serial.println("initBoard");
    pinMode(ADC_PIN, INPUT);
    battery.attach(ADC_PIN);
    voltage = battery.readVoltage() * 2;
    Serial.printf("Battery: %1.2f V\n", voltage);
    if (voltage <= 2.8) {
        ESP.deepSleep(999999999 * 999999999U);
    }

    SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN);

    Wire.begin(I2C_SDA, I2C_SCL, 400000);

#ifdef HAS_AD_BUTTON
    pinMode(BUTTON_PIN, INPUT);
#endif

#ifdef I2C1_SDA
    Wire1.begin(I2C1_SDA, I2C1_SCL);
#endif


#ifdef HAS_GPS
    Serial1.begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
#endif

#if OLED_RST
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, HIGH); delay(20);
    digitalWrite(OLED_RST, LOW);  delay(20);
    digitalWrite(OLED_RST, HIGH); delay(20);
#endif

#if defined(HAS_PMU)
    initPMU();
#endif

#ifdef BOARD_LED
    /*
    * T-BeamV1.0, V1.1 LED defaults to low level as trun on,
    * so it needs to be forced to pull up
    * * * * */
#if LED_ON == LOW
    gpio_hold_dis(GPIO_NUM_4);
#endif
    pinMode(BOARD_LED, OUTPUT);
    digitalWrite(BOARD_LED, LED_ON);
#endif


#ifdef HAS_DISPLAY
    Wire.beginTransmission(0x3C);
    if (Wire.endTransmission() == 0) {
        Serial.println("Started OLED");
        u8g2 = std::unique_ptr<DISPLAY_MODEL>(new DISPLAY_MODEL(U8G2_R0, U8X8_PIN_NONE));
        u8g2->begin();
        u8g2->clearBuffer();
        u8g2->setFlipMode(0);
        u8g2->setFontMode(1); // Transparent
        u8g2->setDrawColor(1);
        u8g2->setFontDirection(0);
        // u8g2->setContrast(255); // todo: add contrast settings.
        u8g2->enableUTF8Print();
        u8g2->firstPage();
        if (voltage < 3.10) {
            u8g2->setFont(FONT_12_GB2312);
            u8g2->drawUTF8(24, 32, "低电压");
            u8g2->sendBuffer();
            ESP.deepSleep(999999999 * 999999999U);
        }
        do {
//            u8g2->setFont(u8g2_font_inb19_mr);
//            u8g2->drawStr(0, 30, "ESP32");
//            u8g2->drawHLine(2, 35, 47);
//            u8g2->drawHLine(3, 36, 47);
//            u8g2->drawVLine(45, 32, 12);
//            u8g2->drawVLine(46, 33, 12);
//            u8g2->setFont(u8g2_font_inb19_mf);
//            u8g2->drawStr(58, 60, "FSK");
//             u8g2->setFont(u8g2_font_luRS19_tr);
            u8g2->setFont(u8g2_font_spleen16x32_mr);
            u8g2->drawStr(13, 45, "Rx_LBJ");
            u8g2->setFont(u8g2_font_spleen8x16_mu);
            u8g2->drawStr(15, 20, "SX1276");
        } while (u8g2->nextPage());
        u8g2->sendBuffer();
        u8g2->setFont(u8g2_font_fur11_tf);
//        delay(1000);
    }
#endif


#ifdef HAS_SDCARD
    if (u8g2) {
        u8g2->setFont(FONT_12_GB2312);
    }
    pinMode(SDCARD_MISO, INPUT_PULLUP);
    SDSPI.begin(SDCARD_SCLK, SDCARD_MISO, SDCARD_MOSI, SDCARD_CS);
//    if (u8g2) {
//        u8g2->clearBuffer();
//    }
    if (!SD.begin(SDCARD_CS, SDSPI, 40000000)) {
        // SD卡初始化失败，但不在屏幕上显示错误信息
        Serial.println("setupSDCard FAIL");

    } else {
        have_sd = true;
        uint32_t cardSize = SD.cardSize() / (1024 * 1024);
        if (u8g2) {
            do {
                u8g2->setCursor(0, 62);
                u8g2->print("SD卡容量:");
                u8g2->print(cardSize / 1024.0);
                u8g2->println(" GB");
            } while (u8g2->nextPage());
        }

        Serial.print("setupSDCard PASS . SIZE = ");
        Serial.print(cardSize / 1024.0);
        Serial.println(" GB");
    }
    if (u8g2) {
        u8g2->sendBuffer();
    }
//    delay(1500);
#endif

#ifdef HAS_DISPLAY
    if (u8g2) {
//        u8g2->clearBuffer();
        do {
            // u8g2->clearBuffer();
            // u8g2->drawXBM(0,0,16,16,bitmap_test);
            // u8g2->sendBuffer();
//            u8g2->setDrawColor(0);
//            u8g2->drawBox(0,53,128,12);
//            u8g2->setDrawColor(1);
//            u8g2->setCursor(0, 62);
//            u8g2->println( "Intializing...");
        } while (u8g2->nextPage());
        // delay(5000);
    }
#endif

#ifdef HAS_RTC
    if(rtc.begin())
        have_rtc = true;
#endif

}

