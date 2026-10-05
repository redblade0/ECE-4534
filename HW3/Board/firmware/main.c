#include "ti_msp_dl_config.h"
#include <stdbool.h>
#include <stdint.h>

static volatile bool sampleFlag = false;

static bool readJoystick(uint16_t *x, uint16_t *y)
{
    DL_ADC12_startConversion(JS_X_INST);

    while (DL_ADC12_getRawInterruptStatus(
        JS_X_INST,
        DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED) == 0) {
    }

    *x = DL_ADC12_getMemResult(JS_X_INST, JS_X_ADCMEM_0);

    DL_ADC12_clearInterruptStatus(
        JS_X_INST,
        DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);

    DL_ADC12_enableConversions(JS_X_INST);

    DL_ADC12_startConversion(JS_Y_INST);

    while (DL_ADC12_getRawInterruptStatus(
        JS_Y_INST,
        DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED) == 0) {
    }

    *y = DL_ADC12_getMemResult(JS_Y_INST, JS_Y_ADCMEM_0);

    DL_ADC12_clearInterruptStatus(
        JS_Y_INST,
        DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);

    DL_ADC12_enableConversions(JS_Y_INST);

    return true;
}




static void sendChar(char c)
{
    while (DL_UART_Main_isBusy(UART_0_INST)) {
    }

    DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t)c);
}

static void sendString(const char *s)
{
    while (*s) {
        sendChar(*s++);
    }
}

static void sendNumber(uint32_t value)
{
    char buffer[10];
    int i = 0;

    if (value == 0) {
        sendChar('0');
        return;
    }

    while (value > 0) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        sendChar(buffer[--i]);
    }
}

static void sendSample(uint32_t number, uint16_t x, uint16_t y)
{
    sendNumber(number);
    sendString(",");
    sendNumber(x);
    sendString(",");
    sendNumber(y);
    sendString("\r\n");
}

void TIMER_0_INST_IRQHandler(void)
{
    switch (DL_TimerA_getPendingInterrupt(TIMER_0_INST)) {
        case DL_TIMERA_IIDX_ZERO:
            sampleFlag = true;
            break;
        default:
            break;
    }
}

int main(void)
{
    uint32_t sampleNumber = 0;

    SYSCFG_DL_init();

    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);
    DL_TimerA_startCounter(TIMER_0_INST);

    while (1) {
        if (sampleFlag) {
            sampleFlag = false;

            uint16_t x;
            uint16_t y;

            if (readJoystick(&x, &y)) {
                sendSample(sampleNumber, x, y);
                sampleNumber++;
            }
        }

        __WFI();
    }
}
