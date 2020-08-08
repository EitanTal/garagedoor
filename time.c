#include <iostm8s003.h>
#include <stdint.h>
#include <stdbool.h>

#define TIM1_AUTO_RELOAD 1000
#define TIM1_PRESCALER 2000

static bool Time_passed;

enum {
    BIT_0 = 1 << 0,
    BIT_1 = 1 << 1,
    BIT_2 = 1 << 2,
    BIT_3 = 1 << 3,
    BIT_4 = 1 << 4,
    BIT_5 = 1 << 5,
    BIT_6 = 1 << 6,
    BIT_7 = 1 << 7
};

enum {
    TIM2_CR1_ENABLE = BIT_0,
    TIM2_CR1_AUTORELOAD = BIT_7,
    TIM2_CCMR_OUTPUT_TOGGLE_MODE = BIT_5 | BIT_4,
    TIM2_CCER1_CC2E = BIT_4,
    TIM2_IER_CC1IE = BIT_1,
    TIM2_IER_UIE = BIT_0,
    TIM2_EGR_UG = BIT_0,
    TIM_PRESCALER_16384 = 0xE,
//    TIM_PRESCALER_8192 = 0xD,
//    TIM_PRESCALER_4096 = 0xC,
    TIM_PRESCALER_2048 = 0xB
//    TIM_PRESCALER_1024 = 0xA
};


void TimersSetup(void)
{
    TIM2_ARRH = 0xFF;
    TIM2_ARRL = 0xFF;

    TIM1_ARRH = TIM1_AUTO_RELOAD >> 8;
    TIM1_ARRL = TIM1_AUTO_RELOAD & 0xFF;
    TIM1_PSCRH = TIM1_PRESCALER >> 8;
    TIM1_PSCRL = TIM1_PRESCALER & 0xFF;
    TIM1_EGR = TIM2_EGR_UG; // Reset timer, clear all the things, and apply the prescaler.
    TIM1_CR1 = TIM2_CR1_ENABLE | TIM2_CR1_AUTORELOAD; // Enable the timer
}

bool IsTimePassed(void)
{
    if (Time_passed)
    {
        Time_passed = false;
        return true;
    }
    return false;
}

void ISR_TIM2_UPDATEOVERFLOW(void)
{
    Time_passed = true;
    TIM2_SR1 = 0; // ACK the event
    TIM2_CR1 = 0; // Disable the timer
}

void SetNotification(int seconds_in_future)
{
    uint16_t ticks = (seconds_in_future * 122); // (roughly)
    if (ticks <= 0) ticks = 1;
    TIM2_CR1 = 0; // Disable the timer
    TIM2_PSCR = TIM_PRESCALER_16384;
    TIM2_CCR1H = ticks >> 8;
    TIM2_CCR1L = ticks & 0xFF;
    TIM2_CNTRH = 0;
    TIM2_CNTRL = 0;
    TIM2_IER = TIM2_IER_CC1IE; // Enable the timer interrupt
    TIM2_EGR = TIM2_EGR_UG; // Reset timer, clear all the things, and apply the prescaler.
    Time_passed = false; // Disable a would-be pending event
    TIM2_CR1 = TIM2_CR1_ENABLE; // Enable the timer
}

int get_milliseconds_now(void)
{
    return TIM1_CNTRL | TIM1_CNTRH << 8;
}

int get_milliseconds_since(int when)
{
    int now = get_milliseconds_now();
    int diff = now - when;
    if (diff < 0) diff += TIM1_AUTO_RELOAD;
    return diff;
}
