#include "Graphics.h"
#include "Display.h"

void Graphics_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= 128 || y >= 128)
    {
        return;
    }

    LCD_SetAddressWindow(x, y, x, y);

    LCD_StartWrite();

    LCD_WritePixel(color);

    LCD_EndWrite();
}

void Graphics_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    int16_t dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int16_t dy = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int16_t sx = (x1 < x2) ? 1 : -1;
    int16_t sy = (y1 < y2) ? 1 : -1;
    int16_t error = dx - dy;
    int16_t error2;

    while (1)
    {
        Graphics_DrawPixel(x1, y1, color);

        if (x1 == x2 && y1 == y2)
        {
            break;
        }

        error2 = 2 * error;

        if (error2 > -dy)
        {
            error -= dy;
            x1 += sx;
        }

        if (error2 < dx)
        {
            error += dx;
            y1 += sy;
        }
    }
}

void Graphics_FillScreen(uint16_t color)
{
    uint32_t i;

    LCD_SetAddressWindow(0, 0, 127, 127);

    LCD_StartWrite();

    for (i = 0; i < 16384UL; i++)
    {
        LCD_WritePixel(color);
    }

    LCD_EndWrite();
}

void Graphics_DrawChar(uint16_t x, uint16_t y, char character, uint16_t color)
{
    uint8_t column;
    uint8_t row;
    uint8_t ascii;

    ascii = (uint8_t)character;

    if (ascii < 32 || ascii > 126)
    {
        return;
    }

    for (column = 0; column < 5; column++)
    {
        for (row = 0; row < 7; row++)
        {
            if (font5x7[ascii][column] & (1U << row))
            {
                Graphics_DrawPixel(x + column, y + row, color);
            }
        }
    }
}

void Graphics_DrawString(uint16_t x, uint16_t y, const char *string, uint16_t color)
{
    while (*string)
    {
        Graphics_DrawChar(x, y, *string, color);
        x += 6;
        string++;
    }
}