#include <stdio.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "ti_msp_dl_config.h"
#include "timestamp.h"


#define EVENT_VARIANT 1
// #define EVENT_VARIANT 2
// #define EVENT_VARIANT 3

#define ACQ_PRIORITY 5
#define EVENT_PRIORITY 4
#define CTRL_PRIORITY 3
#define UI_PRIORITY 2
#define LOG_PRIORITY 1

#define ACQ_PERIOD_MS       1
#define CTRL_PERIOD_MS      10
#define UI_PERIOD_MS        50
#define LOG_PERIOD_MS       200

#define ADC_RING_SIZE       64
#define CTRL_ITERATIONS     10000U

#define ACQ_PERIOD_TICKS    32000U
#define EVENT_PERIOD_TICKS  160000U
#define CTRL_PERIOD_TICKS   320000U
#define UI_PERIOD_TICKS     1600000U
#define LOG_PERIOD_TICKS    6400000U

typedef struct
{
    uint16_t value;
    uint32_t timestamp;
} AdcSample;

#define HIST_BUCKETS 16

typedef struct
{
    uint32_t jobs;
    uint32_t minResponseUs;
    uint32_t maxResponseUs;
    uint32_t totalResponseUs;

    uint32_t minJitterUs;
    uint32_t maxJitterUs;
    uint32_t totalJitterUs;

    uint32_t deadlineMisses;

    uint32_t histogram[HIST_BUCKETS];
} TaskStats;

static TaskStats acqStats;
static TaskStats eventStats;
static TaskStats ctrlStats;
static TaskStats uiStats;
static TaskStats logStats;

static AdcSample adcRing[ADC_RING_SIZE];

static volatile uint32_t adcWriteIndex = 0;
static volatile uint32_t adcReadIndex = 0;
static volatile uint32_t adcDroppedSamples = 0;
static volatile uint32_t adcSampleCount = 0;
static volatile uint16_t lastAdcValue = 0;

volatile uint32_t timerOverflowCount = 0;
volatile uint32_t eventIsrTime = 0;
volatile uint32_t eventIsrCount = 0;

static volatile uint32_t eventLatencyMinUs = UINT32_MAX;
static volatile uint32_t eventLatencyMaxUs = 0;
static volatile uint64_t eventLatencyTotalUs = 0;
static volatile uint32_t eventLatencyCount = 0;
#define EVENT_LATENCY_BUCKETS 64
#define EVENT_LATENCY_MAX_US  5000U

static uint32_t eventLatencyHistogram[EVENT_LATENCY_BUCKETS];
static SemaphoreHandle_t eventSemaphore = NULL;
static TaskHandle_t eventTaskHandle = NULL;

static volatile uint16_t controlOutput = 0;

static volatile uint32_t acqMaxTicks = 0;
static volatile uint32_t eventMaxTicks = 0;
static volatile uint32_t ctrlMaxTicks = 0;
static volatile uint32_t uiMaxTicks = 0;
static volatile uint32_t logMaxTicks = 0;

static void prvSetupHardware(void);

static uint32_t getReleaseTime(TickType_t releaseTick)
{
    TickType_t tick1;
    TickType_t tick2;

    uint32_t now;
    uint32_t systickValue;
    uint32_t systickLoad;
    uint32_t currentTickTime;
    uint32_t tickPeriodTicks;

    int32_t tickDifference;

    do
    {
        tick1 = xTaskGetTickCount();

        now = ts_now();

        systickValue = SysTick->VAL;
        systickLoad = SysTick->LOAD;

        tick2 = xTaskGetTickCount();

    } while (tick1 != tick2);

    currentTickTime =
        now - (systickLoad - systickValue);

    tickPeriodTicks = systickLoad + 1U;

    tickDifference =
        (int32_t)(releaseTick - tick1);

    return currentTickTime +
           ((uint32_t)tickDifference * tickPeriodTicks);
}

static int adcRingPush(uint16_t value, uint32_t timestamp)
{
    uint32_t nextWrite = (adcWriteIndex + 1U) % ADC_RING_SIZE;

    if (nextWrite == adcReadIndex)
    {
        adcDroppedSamples++;
        return 0;
    }

    adcRing[adcWriteIndex].value = value;
    adcRing[adcWriteIndex].timestamp = timestamp;
    adcWriteIndex = nextWrite;

    return 1;
}

static int adcRingPop(AdcSample *sample)
{
    if (adcReadIndex == adcWriteIndex)
    {
        return 0;
    }

    *sample = adcRing[adcReadIndex];
    adcReadIndex = (adcReadIndex + 1U) % ADC_RING_SIZE;

    return 1;
}

static void uart_putc(char c)
{
    DL_UART_Main_transmitDataBlocking(
        UART_0_INST,
        (uint8_t)c);
}

static void uart_puts(const char *str)
{
    while (*str != '\0')
    {
        uart_putc(*str++);
    }
}

static uint32_t ticksToUs(uint32_t ticks)
{
    return (ticks + 16U) / 32U;
}

static TickType_t periodMsToTicks(uint32_t periodMs)
{
    TickType_t ticks;

    ticks = pdMS_TO_TICKS(periodMs);

    if ((periodMs > 0U) && (ticks == 0U))
    {
        ticks = 1U;
    }

    return ticks;
}

static void statsInit(TaskStats *stats)
{
    stats->jobs = 0;
    stats->minResponseUs = UINT32_MAX;
    stats->maxResponseUs = 0;
    stats->totalResponseUs = 0;

    stats->minJitterUs = UINT32_MAX;
    stats->maxJitterUs = 0;
    stats->totalJitterUs = 0;

    stats->deadlineMisses = 0;

    for (uint32_t i = 0; i < HIST_BUCKETS; i++)
    {
        stats->histogram[i] = 0;
    }
}

static void statsRecord(
    TaskStats *stats,
    uint32_t completionTime,
    uint32_t startTime,
    uint32_t releaseTime,
    uint32_t deadlineTicks)
{
    uint32_t responseTicks;
    uint32_t jitterTicks;
    uint32_t responseUs;
    uint32_t jitterUs;
    uint32_t bucket;

    responseTicks = completionTime - releaseTime;
    jitterTicks = startTime - releaseTime;

    responseUs = ticksToUs(responseTicks);
    jitterUs = ticksToUs(jitterTicks);

    stats->jobs++;

    if (responseUs < stats->minResponseUs)
    {
        stats->minResponseUs = responseUs;
    }

    if (responseUs > stats->maxResponseUs)
    {
        stats->maxResponseUs = responseUs;
    }

    stats->totalResponseUs += responseUs;

    if (jitterUs < stats->minJitterUs)
    {
        stats->minJitterUs = jitterUs;
    }

    if (jitterUs > stats->maxJitterUs)
    {
        stats->maxJitterUs = jitterUs;
    }

    stats->totalJitterUs += jitterUs;

    if (responseTicks > deadlineTicks)
    {
        stats->deadlineMisses++;
    }

    bucket = (responseTicks * HIST_BUCKETS) / deadlineTicks;

    if (bucket >= HIST_BUCKETS)
    {
        bucket = HIST_BUCKETS - 1U;
    }

    stats->histogram[bucket]++;
}

static uint32_t statsMeanResponse(const TaskStats *stats)
{
    if (stats->jobs == 0)
    {
        return 0;
    }

    return stats->totalResponseUs / stats->jobs;
}

static uint32_t statsMeanJitter(const TaskStats *stats)
{
    if (stats->jobs == 0)
    {
        return 0;
    }

    return stats->totalJitterUs / stats->jobs;
}

static void updateMax(volatile uint32_t *maxValue, uint32_t value)
{
    if (value > *maxValue)
    {
        *maxValue = value;
    }
}

static uint32_t eventLatencyMeanUs(void)
{
    if (eventLatencyCount == 0)
    {
        return 0;
    }

    return (uint32_t)(
        eventLatencyTotalUs / eventLatencyCount);
}

static uint32_t eventLatencyP99Us(void)
{
    uint32_t target;
    uint32_t cumulative = 0;

    if (eventLatencyCount == 0)
    {
        return 0;
    }

    target =
        (eventLatencyCount * 99U + 99U) / 100U;

    for (uint32_t i = 0;
         i < EVENT_LATENCY_BUCKETS;
         i++)
    {
        cumulative += eventLatencyHistogram[i];

        if (cumulative >= target)
        {
            return ((i + 1U) * EVENT_LATENCY_MAX_US) /
                   EVENT_LATENCY_BUCKETS;
        }
    }

    return EVENT_LATENCY_MAX_US;
}

static uint32_t getTotalUtilizationX100(void)
{
    uint64_t utilization;

    utilization = 0;

    utilization +=
        ((uint64_t)acqMaxTicks * 10000U) /
        ACQ_PERIOD_TICKS;

    utilization +=
        ((uint64_t)eventMaxTicks * 10000U) /
        EVENT_PERIOD_TICKS;

    utilization +=
        ((uint64_t)ctrlMaxTicks * 10000U) /
        CTRL_PERIOD_TICKS;

    utilization +=
        ((uint64_t)uiMaxTicks * 10000U) /
        UI_PERIOD_TICKS;

    utilization +=
        ((uint64_t)logMaxTicks * 10000U) /
        LOG_PERIOD_TICKS;

    return (uint32_t)utilization;
}

void TIMA1_IRQHandler(void)
{
    if (DL_TimerA_getPendingInterrupt(TIMER_0_INST)
        == DL_TIMER_IIDX_ZERO)
    {
        timerOverflowCount++;

        DL_TimerA_clearInterruptStatus(
            TIMER_0_INST,
            DL_TIMERA_INTERRUPT_ZERO_EVENT);
    }
}

void TIMA0_IRQHandler(void)
{
    uint32_t isrEntryTime = ts_now();
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (DL_TimerA_getPendingInterrupt(TIMER_1_INST)
        == DL_TIMER_IIDX_ZERO)
    {
        eventIsrTime = isrEntryTime;
        eventIsrCount++;

    #if EVENT_VARIANT == 3
            vTaskNotifyGiveFromISR(
                eventTaskHandle,
                &xHigherPriorityTaskWoken);
    #else
            xSemaphoreGiveFromISR(
                eventSemaphore,
                &xHigherPriorityTaskWoken);
    #endif

    #if EVENT_VARIANT != 2
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    #endif
        }
}

static void AcqTask(void *pvParameters)
{
    TickType_t lastWakeTime;
    TickType_t releaseTick;
    TickType_t acqPeriodTicks;

    lastWakeTime = xTaskGetTickCount();
    acqPeriodTicks = periodMsToTicks(ACQ_PERIOD_MS);

    (void)pvParameters;

    uart_puts("ACQ TASK STARTED\r\n");

    lastWakeTime = xTaskGetTickCount();

    while (1)
    {
        uint16_t adcValue;
        uint32_t releaseTime;
        uint32_t startTime;
        uint32_t completionTime;

        releaseTick = lastWakeTime;

        releaseTime = getReleaseTime(releaseTick);
        // releaseTime =
        //     baseTime +
        //     ((uint32_t)(releaseTick - baseTick) * 32000U);

        startTime = ts_now();

        DL_ADC12_startConversion(ADC12_0_INST);

        while (DL_ADC12_getRawInterruptStatus(
            ADC12_0_INST,
            DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED) == 0)
        {
        }

        adcValue = DL_ADC12_getMemResult(
            ADC12_0_INST,
            ADC12_0_ADCMEM_0);

        DL_ADC12_clearInterruptStatus(
            ADC12_0_INST,
            DL_ADC12_INTERRUPT_MEM0_RESULT_LOADED);

        DL_ADC12_enableConversions(ADC12_0_INST);

        adcRingPush(
            adcValue,
            ts_now());

        lastAdcValue = adcValue;
        adcSampleCount++;

        completionTime = ts_now();

        statsRecord(
            &acqStats,
            completionTime,
            startTime,
            releaseTime,
            32000U);

        vTaskDelayUntil(
            &lastWakeTime,
            acqPeriodTicks);
    }
}

static void EventTask(void *pvParameters)
{
    (void)pvParameters;

    uart_puts("EVENT TASK STARTED\r\n");

    while (1)
    {
        uint32_t releaseTime;
        uint32_t startTime;
        uint32_t completionTime;
        uint32_t latencyUs;
        uint32_t latencyBucket;
        volatile uint32_t dummy = 0;

        #if EVENT_VARIANT == 3
        ulTaskNotifyTake(
            pdTRUE,
            portMAX_DELAY);
        #else
        xSemaphoreTake(
            eventSemaphore,
            portMAX_DELAY);
        #endif

        releaseTime = eventIsrTime;
        startTime = ts_now();

        latencyUs = ticksToUs(startTime - releaseTime);

        if (latencyUs < eventLatencyMinUs)
        {
            eventLatencyMinUs = latencyUs;
        }

        if (latencyUs > eventLatencyMaxUs)
        {
            eventLatencyMaxUs = latencyUs;
        }

        eventLatencyTotalUs += latencyUs;
        eventLatencyCount++;

        latencyBucket =
            (latencyUs * EVENT_LATENCY_BUCKETS) /
            EVENT_LATENCY_MAX_US;

        if (latencyBucket >= EVENT_LATENCY_BUCKETS)
        {
            latencyBucket = EVENT_LATENCY_BUCKETS - 1U;
        }

        eventLatencyHistogram[latencyBucket]++;

        for (uint32_t i = 0; i < 100U; i++)
        {
            dummy += i;
        }

        (void)dummy;

        completionTime = ts_now();

        statsRecord(
            &eventStats,
            completionTime,
            startTime,
            releaseTime,
            160000U);
    }

}

static void CtrlTask(void *pvParameters)
{
    TickType_t lastWakeTime;
    TickType_t releaseTick;
    TickType_t ctrlPeriodTicks;

    lastWakeTime = xTaskGetTickCount();
    ctrlPeriodTicks = periodMsToTicks(CTRL_PERIOD_MS);

    (void)pvParameters;

    uart_puts("CTRL TASK STARTED\r\n");

    lastWakeTime = xTaskGetTickCount();

    while (1)
    {
        AdcSample sample;
        int32_t filtered = controlOutput;
        volatile uint32_t dummy = 0;

        uint32_t releaseTime;
        uint32_t startTime;
        uint32_t completionTime;

        releaseTick = lastWakeTime;

        // releaseTime =
        //     baseTime +
        //     ((uint32_t)(releaseTick - baseTick) * 32000U);
        releaseTime = getReleaseTime(releaseTick);


        startTime = ts_now();

        while (adcRingPop(&sample))
        {
            filtered =
                (filtered * 3 + (int32_t)sample.value) / 4;
        }

        if (filtered < 0)
        {
            filtered = 0;
        }

        if (filtered > 4095)
        {
            filtered = 4095;
        }

        controlOutput = (uint16_t)filtered;

        for (uint32_t i = 0; i < CTRL_ITERATIONS; i++)
        {
            dummy += (i * 17U) & 0xFFU;
            dummy ^= (i << 2);
        }

        (void)dummy;

        DL_DAC12_output12(
            DAC0,
            controlOutput);

        completionTime = ts_now();

        statsRecord(
            &ctrlStats,
            completionTime,
            startTime,
            releaseTime,
            320000U);

        vTaskDelayUntil(
            &lastWakeTime,
            ctrlPeriodTicks);
    }
}

static void UiTask(void *pvParameters)
{
    TickType_t lastWakeTime;
    TickType_t releaseTick;
    TickType_t uiPeriodTicks;

    uiPeriodTicks = periodMsToTicks(UI_PERIOD_MS);

    uint8_t previousButton = 0;
    uint8_t stableButton = 0;

    (void)pvParameters;

    uart_puts("UI TASK STARTED\r\n");

    lastWakeTime = xTaskGetTickCount();

    while (1)
    {
        uint8_t button;
        uint32_t releaseTime;
        uint32_t startTime;
        uint32_t completionTime;

        releaseTick = lastWakeTime;

        // releaseTime =
        //     baseTime +
        //     ((uint32_t)(releaseTick - baseTick) * 32000U);

        releaseTime = getReleaseTime(releaseTick);


        startTime = ts_now();

        button =
            (DL_GPIO_readPins(
                GPIOA,
                DL_GPIO_PIN_18) != 0) ? 1U : 0U;

        if (button == previousButton)
        {
            stableButton = button;
        }

        previousButton = button;

        DL_GPIO_clearPins(
            GPIOB,
            DL_GPIO_PIN_22 |
            DL_GPIO_PIN_26 |
            DL_GPIO_PIN_27);

        if (stableButton)
        {
            DL_GPIO_setPins(
                GPIOB,
                DL_GPIO_PIN_26);
        }
        else if (controlOutput > 2730)
        {
            DL_GPIO_setPins(
                GPIOB,
                DL_GPIO_PIN_26);
        }
        else if (controlOutput > 1365)
        {
            DL_GPIO_setPins(
                GPIOB,
                DL_GPIO_PIN_27);
        }
        else
        {
            DL_GPIO_setPins(
                GPIOB,
                DL_GPIO_PIN_22);
        }

        completionTime = ts_now();

        statsRecord(
            &uiStats,
            completionTime,
            startTime,
            releaseTime,
            1600000U);

        vTaskDelayUntil(
            &lastWakeTime,
            uiPeriodTicks);
    }
}

static void logHistogram(const char *taskName, const TaskStats *stats)
{
    char buffer[96];

    snprintf(
        buffer,
        sizeof(buffer),
        "HISTOGRAM,%s,BUCKET,COUNT\r\n",
        taskName);

    uart_puts(buffer);

    for (uint32_t i = 0; i < HIST_BUCKETS; i++)
    {
        snprintf(
            buffer,
            sizeof(buffer),
            "HISTOGRAM,%s,%lu,%lu\r\n",
            taskName,
            (unsigned long)i,
            (unsigned long)stats->histogram[i]);

        uart_puts(buffer);
    }

}

static void logEventLatency(void)
{
    char buffer[96];

    uint32_t count;
    uint32_t minUs;
    uint32_t maxUs;
    uint32_t isrCount;
    uint64_t totalUs;
    uint32_t histogram[64];

    taskENTER_CRITICAL();

    count = eventLatencyCount;
    minUs = eventLatencyMinUs;
    maxUs = eventLatencyMaxUs;
    totalUs = eventLatencyTotalUs;
    isrCount = eventIsrCount;

    for (uint32_t i = 0; i < 64U; i++)
    {
        histogram[i] = eventLatencyHistogram[i];
    }

    taskEXIT_CRITICAL();

    uint32_t meanUs = 0;
    uint32_t p99Us = 0;

    if (count > 0U)
    {
        meanUs = (uint32_t)(totalUs / count);

        uint32_t target =
            (count * 99U + 99U) / 100U;

        uint32_t cumulative = 0;

        for (uint32_t i = 0; i < 64U; i++)
        {
            cumulative += histogram[i];

            if (cumulative >= target)
            {
                p99Us =
                    ((i + 1U) * 5000U) / 64U;
                break;
            }
        }
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "LATENCY,EVENT,%lu,%lu,%lu,%lu,%lu,%lu\r\n",
        (unsigned long)count,
        (unsigned long)minUs,
        (unsigned long)maxUs,
        (unsigned long)meanUs,
        (unsigned long)p99Us,
        (unsigned long)isrCount);

    uart_puts(buffer);

    for (uint32_t i = 0; i < 64U; i++)
    {
        snprintf(
            buffer,
            sizeof(buffer),
            "LATENCY_HIST,EVENT,%lu,%lu\r\n",
            (unsigned long)i,
            (unsigned long)histogram[i]);

        uart_puts(buffer);
    }
}

static void LogTask(void *pvParameters)
{
    TickType_t lastWakeTime;
    TickType_t releaseTick;
    TickType_t logPeriodTicks;

    lastWakeTime = xTaskGetTickCount();
    logPeriodTicks = periodMsToTicks(LOG_PERIOD_MS);

    char buffer[256];
    uint8_t histogramDumped = 0;

    (void)pvParameters;

    uart_puts("LOG TASK STARTED\r\n");

    lastWakeTime = xTaskGetTickCount();

    while (1)
    {
        uint32_t releaseTime;
        uint32_t startTime;
        uint32_t completionTime;

        releaseTick = lastWakeTime;

        releaseTime = getReleaseTime(releaseTick);

        startTime = ts_now();

        snprintf(
            buffer,
            sizeof(buffer),
            "STATS,ACQ,%lu,%lu,%lu,%lu,%lu,%lu,%lu\r\n",
            (unsigned long)acqStats.jobs,
            (unsigned long)acqStats.minResponseUs,
            (unsigned long)acqStats.maxResponseUs,
            (unsigned long)statsMeanResponse(&acqStats),
            (unsigned long)acqStats.minJitterUs,
            (unsigned long)statsMeanJitter(&acqStats),
            (unsigned long)acqStats.deadlineMisses);

        uart_puts(buffer);

        snprintf(
            buffer,
            sizeof(buffer),
            "STATS,EVENT,%lu,%lu,%lu,%lu,%lu,%lu,%lu\r\n",
            (unsigned long)eventStats.jobs,
            (unsigned long)eventStats.minResponseUs,
            (unsigned long)eventStats.maxResponseUs,
            (unsigned long)statsMeanResponse(&eventStats),
            (unsigned long)eventStats.minJitterUs,
            (unsigned long)statsMeanJitter(&eventStats),
            (unsigned long)eventStats.deadlineMisses);

        uart_puts(buffer);

        snprintf(
            buffer,
            sizeof(buffer),
            "STATS,CTRL,%lu,%lu,%lu,%lu,%lu,%lu,%lu\r\n",
            (unsigned long)ctrlStats.jobs,
            (unsigned long)ctrlStats.minResponseUs,
            (unsigned long)ctrlStats.maxResponseUs,
            (unsigned long)statsMeanResponse(&ctrlStats),
            (unsigned long)ctrlStats.minJitterUs,
            (unsigned long)statsMeanJitter(&ctrlStats),
            (unsigned long)ctrlStats.deadlineMisses);

        uart_puts(buffer);

        snprintf(
            buffer,
            sizeof(buffer),
            "STATS,UI,%lu,%lu,%lu,%lu,%lu,%lu,%lu\r\n",
            (unsigned long)uiStats.jobs,
            (unsigned long)uiStats.minResponseUs,
            (unsigned long)uiStats.maxResponseUs,
            (unsigned long)statsMeanResponse(&uiStats),
            (unsigned long)uiStats.minJitterUs,
            (unsigned long)statsMeanJitter(&uiStats),
            (unsigned long)uiStats.deadlineMisses);

        uart_puts(buffer);

        snprintf(
            buffer,
            sizeof(buffer),
            "STATS,LOG,%lu,%lu,%lu,%lu,%lu,%lu,%lu\r\n",
            (unsigned long)logStats.jobs,
            (unsigned long)logStats.minResponseUs,
            (unsigned long)logStats.maxResponseUs,
            (unsigned long)statsMeanResponse(&logStats),
            (unsigned long)logStats.minJitterUs,
            (unsigned long)statsMeanJitter(&logStats),
            (unsigned long)logStats.deadlineMisses);

        uart_puts(buffer);

        completionTime = ts_now();

        statsRecord(
            &logStats,
            completionTime,
            startTime,
            releaseTime,
            6400000U);

        if ((logStats.jobs >= 300U) && (histogramDumped == 0U))
        {
            logHistogram("ACQ", &acqStats);
            logHistogram("CTRL", &ctrlStats);
            logEventLatency();

            histogramDumped = 1U;
        }

        vTaskDelayUntil(
            &lastWakeTime,
            logPeriodTicks);
    }

}

static void measureTsNowOverhead(void)
{
    uint32_t minTicks = UINT32_MAX;
    uint32_t start;
    uint32_t end;
    uint32_t delta;
    char buffer[64];

    for (uint32_t i = 0; i < 1000U; i++)
    {
        start = ts_now();
        end = ts_now();

        delta = end - start;

        if (delta < minTicks)
        {
            minTicks = delta;
        }
    }

    snprintf(
        buffer,
        sizeof(buffer),
        "TS_NOW_OVERHEAD,%lu ticks,%lu us\r\n",
        (unsigned long)minTicks,
        (unsigned long)((minTicks + 16U) / 32U));

    uart_puts(buffer);
}

int main(void)
{
    prvSetupHardware();

    measureTsNowOverhead();

    statsInit(&acqStats);
    statsInit(&eventStats);
    statsInit(&ctrlStats);
    statsInit(&uiStats);
    statsInit(&logStats);

    uart_puts("STARTING FIVE TASK SYSTEM\r\n");

    char variantBuffer[64];

    snprintf(
        variantBuffer,
        sizeof(variantBuffer),
        "EVENT_VARIANT,%d\r\n",
        EVENT_VARIANT);

    uart_puts(variantBuffer);

    NVIC_SetPriority(
        TIMER_0_INST_INT_IRQN,
        1);

    NVIC_SetPriority(
        TIMER_1_INST_INT_IRQN,
        2);

    NVIC_EnableIRQ(
        TIMER_0_INST_INT_IRQN);

    NVIC_EnableIRQ(
        TIMER_1_INST_INT_IRQN);

    eventSemaphore = xSemaphoreCreateBinary();

    if (eventSemaphore == NULL)
    {
        while (1)
        {
        }
    }

    xTaskCreate(
        AcqTask,
        "AcqTask",
        512,
        NULL,
        ACQ_PRIORITY,
        NULL);

    xTaskCreate(
        EventTask,
        "EventTask",
        512,
        NULL,
        EVENT_PRIORITY,
        &eventTaskHandle);

    xTaskCreate(
        CtrlTask,
        "CtrlTask",
        768,
        NULL,
        CTRL_PRIORITY,
        NULL);

    xTaskCreate(
        UiTask,
        "UiTask",
        512,
        NULL,
        UI_PRIORITY,
        NULL);

    xTaskCreate(
        LogTask,
        "LogTask",
        1024,
        NULL,
        LOG_PRIORITY,
        NULL);

    DL_TimerA_startCounter(TIMER_0_INST);
    DL_TimerA_startCounter(TIMER_1_INST);
    measureTsNowOverhead();

    vTaskStartScheduler();

    while (1)
    {
    }

    return 0;
}

static void prvSetupHardware(void)
{
    SYSCFG_DL_init();
}

#if (configCHECK_FOR_STACK_OVERFLOW)

#if defined(__IAR_SYSTEMS_ICC__)
__weak void vApplicationStackOverflowHook(
    TaskHandle_t pxTask,
    char *pcTaskName)
#elif defined(__TI_COMPILER_VERSION__)
#pragma WEAK(vApplicationStackOverflowHook)
void vApplicationStackOverflowHook(
    TaskHandle_t pxTask,
    char *pcTaskName)
#elif defined(__GNUC__) || defined(__ti_version__)
void __attribute__((weak))
vApplicationStackOverflowHook(
    TaskHandle_t pxTask,
    char *pcTaskName)
#endif
{
    (void)pxTask;
    (void)pcTaskName;

    while (1)
    {
        DL_GPIO_togglePins(
            GPIOA,
            DL_GPIO_PIN_0);

        for (volatile uint32_t i = 0; i < 100000U; i++)
        {
            __asm("nop");
        }
    }
}

#endif