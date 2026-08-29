#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

void Graphics_Init(void);

void LCD_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_StartWrite(void);
void LCD_EndWrite(void);
void LCD_WritePixel(uint16_t color);

#endif