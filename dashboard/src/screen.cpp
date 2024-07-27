#include "screen.h"

U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ PB_6, /* data=*/ PB_7, /* cs=*/ U8X8_PIN_NONE, /* dc=*/ PB_8, /* reset=*/ PB_9);

void drawSettingHeader(const char *text) {
    u8g2.setFont(FONT_ICON);
    u8g2.drawStr(0, CH, "\x47");
    u8g2.setFont(FONT_STD);
    u8g2.drawStr(CW + CW/2, CH, text);
}

void drawStrCentered(u8g2_uint_t y, const char *text) {
    u8g2_uint_t x = (SCREEN_WIDTH - strlen(text)*CW) / 2;
    u8g2.drawStr(x, y, text);
}

void drawStrFull(const char *text) {
    u8g2.clearBuffer();
    u8g2_uint_t x = (SCREEN_WIDTH - strlen(text)*CW) / 2;
    drawStrCentered((SCREEN_HEIGHT - CH) / 2 + CH, text);
    u8g2.sendBuffer();
}
