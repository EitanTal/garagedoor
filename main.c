/* MAIN.C file
 * 
 * Copyright (c) 2002-2005 STMicroelectronics
 */

#include <stdint.h>
#include <stdbool.h>
#include <iostm8s003.h>


#define BLUE_LED_PIN      (1 << 2)
#define BLUE_LED_PORT      PD_ODR
#define RED_LED_PIN       (1 << 3)
#define RED_LED_PORT       PD_ODR
#define DOOR_SENSOR_PIN   (1 << 6)
#define DOOR_SENSOR_PORT   PC_IDR
#define DOOR_SWITCH_PIN   (1 << 3)
#define DOOR_SWITCH_PORT   PC_ODR

static int State_WatchDoor(void);
static int State_Wait2Minutes(void);
static int State_IssueClose(void);
static int State_IssueClose1(void);
static int State_IssueClose2(void);
static int State_CloseError(void);
static int State_IssueOpen(void);
static int State_Idle(void);
static int State_OpenError(void);
static void LedTask(void);
static void UartTask(void);
static void SensorTask(void);


bool RxCommand_open;
bool RxCommand_close;
bool Sensor_closed;
bool Sensor_open;
bool Time_passed;

#define BLUE_LED_ON()   BLUE_LED_PORT &= ~BLUE_LED_PIN
#define BLUE_LED_OFF()  BLUE_LED_PORT |=  BLUE_LED_PIN

#define RED_LED_ON()    RED_LED_PORT &= ~RED_LED_PIN
#define RED_LED_OFF()   RED_LED_PORT |=  RED_LED_PIN

#define LED_OFF()       (BLUE_LED_PORT |=  RED_LED_PIN | BLUE_LED_PIN)

#define GET_SENSOR()  (DOOR_SENSOR_PORT & DOOR_SENSOR_PIN)
#define GET_SENSOR_BOOL()  (!!(GET_SENSOR()))

void main()
{
    int i;
    int j;

    LED_OFF();
    PD_DDR     |= BLUE_LED_PIN | RED_LED_PIN;

    for (;;)
    {
        for (i = 0; i < 255; i++)
        {
            for (j = 0; j < 255; j++)
            {
                if (j < i && GET_SENSOR())
                {
									BLUE_LED_ON();
                }
                else
                {
                    BLUE_LED_OFF();
                }
            }
        }
				LED_OFF();
#if 1			
        for (i = 0; i < 255; i++)
        {
            for (j = 0; j < 255; j++)
            {
                if (j*8 < i)
                {
                    RED_LED_ON();
                }
                else
                {
                    RED_LED_OFF();
                }
            }
        }
				LED_OFF();
#endif
    }

    {
        int (*state)(void) = State_WatchDoor;
        int next;
        for (;;)
        {
            next = state();
            LedTask();
            UartTask();
            SensorTask();
            state = (int(*)(void))next;
        }
    }
}

int State_WatchDoor()
{
    if (RxCommand_open)
    {
        return (int)State_IssueOpen;
    }
    if (Sensor_open)
    {
        return (int)State_Wait2Minutes;
    }
    return (int)State_WatchDoor;
}

int State_IssueOpen()
{
    if (Time_passed)
    {
        return (int)State_OpenError;
    }
    if (Sensor_open)
    {
        return (int)State_Idle;
    }
    return (int)State_IssueOpen;
}

int State_OpenError()
{
    if (Sensor_open)
    {
        return (int)State_Idle;
    }
}

int State_Idle()
{
    if (RxCommand_close)
    {
        return (int)State_IssueClose;
    }
    if (Sensor_closed)
    {
        return (int)State_WatchDoor;
    }
    return (int)State_Idle;
}

int State_Wait2Minutes()
{
    if (Sensor_closed)
    {
        return (int)State_WatchDoor;
    }
    if (Time_passed)
    {
        return (int)State_IssueClose;
    }
    return (int)State_Wait2Minutes;
}

int State_IssueClose()
{
    // Actually issue close, (close the relay for XXX ms)
    
    return (int)State_IssueClose1;
}

int State_IssueClose1()
{
    if (Sensor_closed)
    {
        return (int)State_WatchDoor;
    }
    if (Time_passed)
    {
        return (int)State_IssueClose2;
    }
    return (int)State_IssueClose1;
}

int State_IssueClose2()
{
    if (Sensor_closed)
    {
        return (int)State_WatchDoor;
    }
    if (Time_passed)
    {
        return (int)State_CloseError;
    }
    return (int)State_IssueClose2;
}

int State_CloseError()
{
    if (Sensor_closed)
    {
        return (int)State_WatchDoor;
    }

    return (int)State_CloseError;
}

void LedTask(void)
{
}

void UartTask(void)
{
}

void SensorTask(void)
{
}
