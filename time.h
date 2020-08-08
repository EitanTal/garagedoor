#pragma once
#include <stdbool.h>

void TimersSetup(void);

bool IsTimePassed(void);

void SetNotification(int seconds_in_future);

int get_milliseconds_now(void);

int get_milliseconds_since(int when);

void ISR_TIM2_UPDATEOVERFLOW(void);
