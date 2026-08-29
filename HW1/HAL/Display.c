// File for setting up the LCD on the MSPM0G3507 with the BoosterPack MKII
// Author: Aiden Wiehn

#include "Display.h"
#include "Graphics.h"

#include <ti/devices/msp/msp.h>
#include <ti/driverlib/driverlib.h>

// LCD control pins
#define LCD_CS_PORT       GPIOB
#define LCD_CS_PIN        DL_GPIO_PIN_6
#define LCD_RESET_PORT    GPIOB
#define LCD_RESET_PIN     DL_GPIO_PIN_15
#define LCD_DC_PORT       GPIOA
#define LCD_DC_PIN        DL_GPIO_PIN_13
#define LCD_BL_PORT       GPIOB
#define LCD_BL_PIN        DL_GPIO_PIN_1
#define LCD_SPI           SPI1

// ST7735 commands
#define ST7735_SWRESET    0x01
#define ST7735_SLPOUT     0x11
#define ST7735_GAMSET     0x26
#define ST7735_SETPWCTR   0xC0
#define ST7735_SETSTBA    0xC4
#define ST7735_COLMOD     0x3A
#define ST7735_MADCTL     0x36
#define ST7735_NORON      0x13
#define ST7735_CASET      0x2A
#define ST7735_RASET      0x2B
#define ST7735_RAMWR      0x2C
#define ST7735_DISPON     0x29

static void Graphics_DelayMs(uint32_t ms)
{
    while (ms--)
    {
        delay_cycles(32000);
    }
}

void LCD_Select(void)
{
    DL_GPIO_clearPins(LCD_CS_PORT, LCD_CS_PIN);
}

void LCD_Deselect(void)
{
    DL_GPIO_setPins(LCD_CS_PORT, LCD_CS_PIN);
}

void LCD_CommandMode(void)
{
    DL_GPIO_clearPins(LCD_DC_PORT, LCD_DC_PIN);
}

void LCD_DataMode(void)
{
    DL_GPIO_setPins(LCD_DC_PORT, LCD_DC_PIN);
}

void LCD_SPI_Write(uint8_t data)
{
    // Send one byte
    DL_SPI_transmitData8(LCD_SPI, data);

    // Wait for transmission
    while (DL_SPI_isBusy(LCD_SPI))
    {
    }

    // Clear received data
    while (!DL_SPI_isRXFIFOEmpty(LCD_SPI))
    {
        (void)DL_SPI_receiveData8(LCD_SPI);
    }
}

static void LCD_WriteCommand(uint8_t command)
{
    LCD_Select();
    LCD_CommandMode();
    LCD_SPI_Write(command);
    LCD_Deselect();
}

static void LCD_WriteCommandData(uint8_t command, const uint8_t *data, uint32_t length)
{
    uint32_t i;

    LCD_Select();
    LCD_CommandMode();
    LCD_SPI_Write(command);
    LCD_DataMode();

    for (i = 0; i < length; i++)
    {
        LCD_SPI_Write(data[i]);
    }

    LCD_Deselect();
}

static void LCD_GPIO_Init(void)
{
    // Configure chip select
    DL_GPIO_initDigitalOutput(IOMUX_PINCM23);
    DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_6);
    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_6);

    // Configure reset
    DL_GPIO_initDigitalOutput(IOMUX_PINCM32);
    DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_15);
    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_15);

    // Configure DC/RS
    DL_GPIO_initDigitalOutput(IOMUX_PINCM35);
    DL_GPIO_enableOutput(GPIOA, DL_GPIO_PIN_13);
    DL_GPIO_setPins(GPIOA, DL_GPIO_PIN_13);

    // Configure backlight
    DL_GPIO_initDigitalOutput(IOMUX_PINCM13);
    DL_GPIO_enableOutput(GPIOB, DL_GPIO_PIN_1);
    DL_GPIO_setPins(GPIOB, DL_GPIO_PIN_1);
}

// Configure SPI1
// PB8 = MOSI
// PB9 = SCLK
// SPI mode 0
// 8-bit MSB first
// CS is manually controlled

static void LCD_SPI_Init(void)
{
    static const DL_SPI_Config spiConfig =
    {
        .mode = DL_SPI_MODE_CONTROLLER,
        .frameFormat = DL_SPI_FRAME_FORMAT_MOTO4_POL0_PHA0,
        .parity = DL_SPI_PARITY_NONE,
        .dataSize = DL_SPI_DATA_SIZE_8,
        .bitOrder = DL_SPI_BIT_ORDER_MSB_FIRST,
        .chipSelectPin = DL_SPI_CHIP_SELECT_NONE
    };

    static const DL_SPI_ClockConfig clockConfig =
    {
        .clockSel = DL_SPI_CLOCK_BUSCLK,
        .divideRatio = DL_SPI_CLOCK_DIVIDE_RATIO_1
    };

    // Configure SPI1 MOSI
    DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM25, IOMUX_PINCM25_PF_SPI1_PICO);

    // Configure SPI1 SCLK
    DL_GPIO_initPeripheralOutputFunction(IOMUX_PINCM26, IOMUX_PINCM26_PF_SPI1_SCLK);

    // Set SPI clock
    DL_SPI_setClockConfig(LCD_SPI, &clockConfig);

    // Configure SPI
    DL_SPI_init(LCD_SPI, &spiConfig);

    // Enable SPI
    DL_SPI_enable(LCD_SPI);
}

static void LCD_Reset(void)
{
    // Assert reset
    DL_GPIO_clearPins(LCD_RESET_PORT, LCD_RESET_PIN);

    Graphics_DelayMs(50);

    // Release reset
    DL_GPIO_setPins(LCD_RESET_PORT, LCD_RESET_PIN);

    Graphics_DelayMs(120);
}

// Set LCD drawing window
void LCD_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    x0 += 2;
    x1 += 2;

    y0 += 3;
    y1 += 3;

    // Set column range
    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)x0;
    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)x1;

    LCD_WriteCommandData(ST7735_CASET, data, 4);

    // Set row range
    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)y0;
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)y1;

    LCD_WriteCommandData(ST7735_RASET, data, 4);
}

// Initialize ST7735
static void LCD_Controller_Init(void)
{
    uint8_t data;

    // Software reset
    LCD_WriteCommand(ST7735_SWRESET);
    Graphics_DelayMs(150);

    // Exit sleep mode
    LCD_WriteCommand(ST7735_SLPOUT);
    Graphics_DelayMs(200);

    // Set gamma
    data = 0x04;
    LCD_WriteCommandData(ST7735_GAMSET, &data, 1);

    // Set power control
    LCD_Select();
    LCD_CommandMode();
    LCD_SPI_Write(ST7735_SETPWCTR);
    LCD_DataMode();
    LCD_SPI_Write(0x0A);
    LCD_SPI_Write(0x14);
    LCD_Deselect();

    // Set display timing
    LCD_Select();
    LCD_CommandMode();
    LCD_SPI_Write(ST7735_SETSTBA);
    LCD_DataMode();
    LCD_SPI_Write(0x0A);
    LCD_SPI_Write(0x00);
    LCD_Deselect();

    // Set RGB565
    data = 0x05;
    LCD_WriteCommandData(ST7735_COLMOD, &data, 1);
    Graphics_DelayMs(10);

    // Set BGR color order
    data = 0xC8;
    LCD_WriteCommandData(ST7735_MADCTL, &data, 1);

    // Enable normal display mode
    LCD_WriteCommand(ST7735_NORON);

    // Set full display window
    LCD_SetAddressWindow(0, 0, 127, 127);

    // Start RAM write
    LCD_WriteCommand(ST7735_RAMWR);

    // Clear display to white
    LCD_Select();
    LCD_DataMode();

    uint32_t i;

    for (i = 0; i < 16384UL; i++)
    {
        LCD_SPI_Write(0xFF);
        LCD_SPI_Write(0xFF);
    }

    LCD_Deselect();

    Graphics_DelayMs(10);

    // Turn display on
    LCD_WriteCommand(ST7735_DISPON);
    Graphics_DelayMs(100);
}

void Graphics_Init(void)
{
    // Enable GPIO power
    DL_GPIO_enablePower(GPIOA);
    DL_GPIO_enablePower(GPIOB);

    // Enable SPI1 power
    DL_SPI_enablePower(SPI1);

    // Wait for peripherals
    delay_cycles(16);

    // Initialize LCD GPIO
    LCD_GPIO_Init();

    // Initialize SPI1
    LCD_SPI_Init();

    // Reset LCD
    LCD_Reset();

    // Initialize LCD controller
    LCD_Controller_Init();

    // Turn on backlight
    DL_GPIO_setPins(LCD_BL_PORT, LCD_BL_PIN);

    //
    Graphics_FillScreen(GRAPHICS_BLACK);
}

void LCD_StartWrite(void)
{
    LCD_Select();
    LCD_CommandMode();
    LCD_SPI_Write(ST7735_RAMWR);
    LCD_DataMode();
}

void LCD_EndWrite(void)
{
    LCD_Deselect();
}

void LCD_WritePixel(uint16_t color)
{
    LCD_SPI_Write((uint8_t)(color >> 8));
    LCD_SPI_Write((uint8_t)color);
}