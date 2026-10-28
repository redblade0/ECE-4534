/*
 * Copyright (c) 2023, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Adapted from TI MSPM0 SDK 2.11.00.07 adc_to_uart.
 * Original TI license is retained in ../baseline/adc_to_uart.c and LICENSE_TI.txt.
 * The TI example provides the board startup and UART pattern.
 */

#include "ti_msp_dl_config.h"
#include "tilt_logic.h"
#include <stdio.h>
#include <string.h>

#define SAMPLE_RATE_HZ 50U
#define CALIBRATION_SAMPLES 64U

volatile uint32_t gSampleTicks = 0;
volatile uint32_t gMissedSamples = 0;
volatile uint32_t gAdcTimeouts = 0;

static bool readAccel(uint16_t *x, uint16_t *y, uint16_t *z,
    uint32_t deadlineTick);
static void sendLine(const char *line);
static void setLed(LedCommand led);
static void sendTelemetry(uint32_t sampleNumber, uint16_t x, uint16_t y,
    uint16_t z, const TiltState *state, bool valid);

int main(void)
{
    uint32_t lastTick = 0;
    uint32_t xSum = 0;
    uint32_t ySum = 0;
    uint32_t zSum = 0;
    uint32_t goodCalibrationSamples = 0;
    TiltState tilt;

    SYSCFG_DL_init();

    /* ADC1 scans X on MEM0 and Z on MEM1. ADC0 samples Y on MEM0. */
    DL_ADC12_disableConversions(ADC12_1_INST);
    DL_ADC12_initSeqSample(ADC12_1_INST,
        DL_ADC12_REPEAT_MODE_DISABLED,
        DL_ADC12_SAMPLING_SOURCE_AUTO,
        DL_ADC12_TRIG_SRC_SOFTWARE,
        DL_ADC12_SEQ_START_ADDR_00,
        DL_ADC12_SEQ_END_ADDR_01,
        DL_ADC12_SAMP_CONV_RES_12_BIT,
        DL_ADC12_SAMP_CONV_DATA_FORMAT_UNSIGNED);
    DL_ADC12_configConversionMem(ADC12_1_INST, DL_ADC12_MEM_IDX_1,
        DL_ADC12_INPUT_CHAN_5,
        DL_ADC12_REFERENCE_VOLTAGE_VDDA,
        DL_ADC12_SAMPLE_TIMER_SOURCE_SCOMP0,
        DL_ADC12_AVERAGING_MODE_DISABLED,
        DL_ADC12_BURN_OUT_SOURCE_DISABLED,
        DL_ADC12_TRIGGER_MODE_AUTO_NEXT,
        DL_ADC12_WINDOWS_COMP_MODE_DISABLED);
    DL_ADC12_enableConversions(ADC12_1_INST);

    /* SysTick uses the 32 MHz CPU clock: 640,000 cycles = 20 milliseconds. */
    if (SysTick_Config(CPUCLK_FREQ / SAMPLE_RATE_HZ) != 0U) {
        while (1) {} /* The requested timer period does not fit. */
    }

    /* Place the BoosterPack level during the first 1.28 seconds after reset. */
    for (uint32_t i = 0; i < CALIBRATION_SAMPLES; ++i) {
        while (gSampleTicks == lastTick) __WFE();
        uint32_t tick = gSampleTicks;
        lastTick = tick;
        uint16_t x, y, z;
        if (readAccel(&x, &y, &z, tick + 1U)) {
            xSum += x;
            ySum += y;
            zSum += z;
            ++goodCalibrationSamples;
        } else {
            ++gAdcTimeouts;
        }
    }
    if (goodCalibrationSamples == 0U) goodCalibrationSamples = 1U;
    tiltInit(&tilt, (int16_t) (xSum / goodCalibrationSamples),
        (int16_t) (ySum / goodCalibrationSamples),
        (int16_t) (zSum / goodCalibrationSamples));
    char calibrationLine[48];
    (void) snprintf(calibrationLine, sizeof(calibrationLine),
        "CAL,%d,%d,%lu\r\n", tilt.zeroX, tilt.zeroY,
        (unsigned long) (zSum / goodCalibrationSamples));
    sendLine(calibrationLine);
    sendLine("D,tick,x,y,z,roll10,pitch10,candidate,stable,step,event,led,valid\r\n");

    while (1) {
        /* The interrupt sends an event, so an early tick cannot be lost here. */
        while (gSampleTicks == lastTick) {
            __WFE();
        }

        uint32_t tick = gSampleTicks;
        gMissedSamples += tick - lastTick - 1U;
        lastTick = tick;

        uint16_t x = 0;
        uint16_t y = 0;
        uint16_t z = 0;
        bool valid = readAccel(&x, &y, &z, tick + 1U);
        if (!valid) {
            ++gAdcTimeouts;
        } else {
            tiltUpdate(&tilt, x, y, z);
            setLed(tilt.led);
        }
        sendTelemetry(tick - 1U, x, y, z, &tilt, valid);
    }
}

void SysTick_Handler(void)
{
    ++gSampleTicks;
    __SEV();
}

static bool readAccel(uint16_t *x, uint16_t *y, uint16_t *z,
    uint32_t deadlineTick)
{
    enum { Y_HISTORY_DEPTH = 6 };
    static uint16_t yHistory[Y_HISTORY_DEPTH];
    static uint8_t yHistoryHead = 0U;
    static bool yHistoryReady = false;
    const uint32_t yReady = DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED;
    const uint32_t zReady = DL_ADC12_INTERRUPT_MEM1_RESULT_LOADED;

    DL_ADC12_clearInterruptStatus(ADC12_0_INST, yReady);
    DL_ADC12_clearInterruptStatus(ADC12_1_INST,
        DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED | zReady);
    DL_ADC12_enableConversions(ADC12_0_INST);
    DL_ADC12_enableConversions(ADC12_1_INST);
    DL_ADC12_startConversion(ADC12_0_INST);
    DL_ADC12_startConversion(ADC12_1_INST);

    while (DL_ADC12_getRawInterruptStatus(ADC12_0_INST, yReady) == 0U) {
        if (gSampleTicks >= deadlineTick) return false;
    }
    while (DL_ADC12_getRawInterruptStatus(ADC12_1_INST, zReady) == 0U) {
        if (gSampleTicks >= deadlineTick) return false;
    }

    uint16_t currentY = DL_ADC12_getMemResult(ADC12_0_INST, DL_ADC12_MEM_IDX_0);
    if (!yHistoryReady) {
        for (uint8_t i = 0U; i < Y_HISTORY_DEPTH; ++i) {
            yHistory[i] = currentY;
        }
        yHistoryReady = true;
    }
    *x = DL_ADC12_getMemResult(ADC12_1_INST, DL_ADC12_MEM_IDX_0);
    *y = currentY;
    yHistory[yHistoryHead] = currentY;
    yHistoryHead = (uint8_t) ((yHistoryHead + 1U) % Y_HISTORY_DEPTH);
    *z = DL_ADC12_getMemResult(ADC12_1_INST, DL_ADC12_MEM_IDX_1);
    return true;
}

static void setLed(LedCommand led)
{
    DL_GPIO_clearPins(BOOST_RGB_R_PORT, BOOST_RGB_R_RED_PIN);
    DL_GPIO_clearPins(BOOST_RGB_G_PORT,
        BOOST_RGB_G_GREEN_PIN | BOOST_RGB_B_BLUE_PIN);
    if (led == LED_RED) {
        DL_GPIO_setPins(BOOST_RGB_R_PORT, BOOST_RGB_R_RED_PIN);
    } else if (led == LED_GREEN) {
        DL_GPIO_setPins(BOOST_RGB_G_PORT, BOOST_RGB_G_GREEN_PIN);
    } else if (led == LED_BLUE) {
        DL_GPIO_setPins(BOOST_RGB_B_PORT, BOOST_RGB_B_BLUE_PIN);
    }
}

static void sendTelemetry(uint32_t sampleNumber, uint16_t x, uint16_t y,
    uint16_t z, const TiltState *state, bool valid)
{
    char line[100];
    int length = snprintf(line, sizeof(line),
        "D,%lu,%u,%u,%u,%d,%d,%c,%c,%u,%c,%c,%u\r\n",
        (unsigned long) sampleNumber, (unsigned int) x, (unsigned int) y,
        (unsigned int) z, state->rollTenths, state->pitchTenths,
        (char) state->candidate, (char) state->stable,
        (unsigned int) state->step, (char) state->event, (char) state->led,
        valid ? 1U : 0U);

    if (length <= 0 || (unsigned int) length >= sizeof(line)) {
        return;
    }
    sendLine(line);
}

static void sendLine(const char *line)
{
    int length = (int) strlen(line);
    for (int i = 0; i < length; ++i) {
        DL_UART_Main_transmitDataBlocking(UART_0_INST, (uint8_t) line[i]);
    }
}

