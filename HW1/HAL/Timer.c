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
    /*
     * Check that the zero-event interrupt caused this interrupt.
     */
    if (DL_TimerA_getPendingInterrupt(TIMER_0_INST)
            == DL_TIMERA_IIDX_ZERO)
    {
        systemMillis++;
    }
}


/*
 * Initialize and start the system timer.
 */
void InitSystemTiming(void)
{
    systemMillis = 0;

    /*
     * Enable the TIMER_0 interrupt in the NVIC.
     */
    NVIC_EnableIRQ(TIMER_0_INST_INT_IRQN);

    /*
     * Start TIMER_0.
     */
    DL_TimerA_startCounter(TIMER_0_INST);
}


/*
 * Return the number of milliseconds since the timer was started.
 */
uint32_t Timer_getMillis(void)
{
    return systemMillis;
}