#ifndef HAL_TIMER_H_
#define HAL_TIMER_H_

#include <stdint.h>
#include <stdbool.h>

#include "ti_msp_dl_config.h"

#define TIMER_PERIOD_MS    1

/*
 * Software timer.
 *
 * All software timers use the 1 ms hardware timer as their time reference.
 */
struct _SWTimer
{
    uint32_t startTime;
    uint32_t waitTime;
};

typedef struct _SWTimer SWTimer;


/*
 * Initializes TIMER_0 and starts the system millisecond counter.
 */
void InitSystemTiming(void);


/*
 * Returns the number of milliseconds since the system timer started.
 */
uint32_t Timer_getMillis(void);


/*
 * Creates a software timer with a specified duration in milliseconds.
 */
SWTimer SWTimer_construct(uint32_t waitTime_ms);


/*
 * Starts/restarts a software timer.
 */
void SWTimer_start(SWTimer* timer_p);


/*
 * Returns true if the software timer has expired.
 */
bool SWTimer_expired(SWTimer* timer_p);


/*
 * Returns the number of milliseconds elapsed since the timer started.
 */
uint32_t SWTimer_elapsedTimeMS(SWTimer* timer_p);

#endif /* HAL_TIMER_H_ */