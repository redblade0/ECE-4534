#ifndef HAL_TIMER_H_
#define HAL_TIMER_H_

#include <stdint.h>
#include <stdbool.h>

#include "ti_msp_dl_config.h"

/*
 * Timer 0 runs at 32 MHz with a 1 ms period.
 */
#define TIMER_PERIOD_MS    1

/*
 * Initializes the system timer.
 *
 * This function enables the timer interrupt and starts TIMER_0.
 */
void InitSystemTiming(void);

/*
 * Returns the number of milliseconds that have elapsed
 * since InitSystemTiming() was called.
 */
uint32_t Timer_getMillis(void);

#endif /* HAL_TIMER_H_ */