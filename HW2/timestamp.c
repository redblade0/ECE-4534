#include <stdint.h>

#include "timestamp.h"
#include "ti_msp_dl_config.h"


extern volatile uint32_t timerOverflowCount;


uint32_t ts_now(void)
{
    uint32_t overflow1;
    uint32_t overflow2;
    uint32_t counter;

    do
    {
        overflow1 = timerOverflowCount;

        counter = DL_TimerA_getTimerCount(
            TIMER_0_INST);

        overflow2 = timerOverflowCount;

    } while (overflow1 != overflow2);

    return (overflow1 << 16) | (counter & 0xFFFFU);
}