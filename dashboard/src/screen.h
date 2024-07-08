#ifndef SCREEN_H
#define SCREEN_H

#include <U8g2lib.h>

// Font parameters
#define FONT_STD  u8g2_font_crox3c_mf // https://github.com/olikraus/u8g2/wiki/fntgrpcrox#crox3h
#define FONT_ICON u8g2_font_twelvedings_t_all // https://github.com/olikraus/u8g2/wiki/fntgrpgeoff#twelvedings
#define CH (12+5) // height of character
#define CW 12 // width of character

// Screen parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

extern U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI u8g2;

extern void drawSettingHeader(const char *text);

extern void drawStrCentered(u8g2_uint_t y, const char *text);

extern void drawStrCentered2(const char *text);

#endif
