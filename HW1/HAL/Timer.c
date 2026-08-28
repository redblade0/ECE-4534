#include <HAL/Timer.h>


/*
 * Millisecond counter.
 *
 * This is incremented once every 1 ms by the TIMER_0 interrupt.
 */
static volatile uint32_t systemMillis = 0;


/*
 * TIMER_0 interrupt service routine.
 *
 * SysConfig defines TIMER_0_INST_IRQHandler as TIMA0_IRQHandler,
 * so this function is actually compiled as TIMA0_IRQHandler.
 */
void TIMER_0_INST_IRQHandler(void)
{
    if (DL_TimerA_getPendingInterrupt(TIMER_0_INST)
            == DL_TIMERA_IIDX_ZERO)
    {
        systemMillis++;
    }
}


/*
 * Initialize and start the hardware timer.
 */
void InitSystemTiming(void)
{
    systemMillis = 0;

    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);

    DL_TimerA_startCounter(TIMER_0_INST);
}


/*
 * Return the current system time in milliseconds.
 */
uint32_t Timer_getMillis(void)
{
    return systemMillis;
}


/*
 * Construct a software timer.
 */
SWTimer SWTimer_construct(uint32_t waitTime_ms)
{
    SWTimer timer;

    timer.startTime = 0;
    timer.waitTime = waitTime_ms;

    return timer;
}


/*
 * Start/restart a software timer.
 */
void SWTimer_start(SWTimer* timer_p)
{
    timer_p->startTime = Timer_getMillis();
}


/*
 * Determine whether a software timer has expired.
 */
bool SWTimer_expired(SWTimer* timer_p)
{
    return (Timer_getMillis() - timer_p->startTime) >= timer_p->waitTime;
}


/*
 * Return the number of milliseconds elapsed since the timer started.
 */
uint32_t SWTimer_elapsedTimeMS(SWTimer* timer_p)
{
    return Timer_getMillis() - timer_p->startTime;
}