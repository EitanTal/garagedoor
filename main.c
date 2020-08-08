#include <stdint.h>
#include <stdbool.h>
#include <iostm8s003.h>
#include "MyPeripherals.h"
#include "time.h"

#define INTERRUPT_EN()   __asm("RIM")
#define INTERRUPT_DIS()  __asm("SIM")

/// States...
static int State_WatchDoor(void);
static int State_Wait2Minutes(void);
static int State_Door_Closing(void);
static int State_Close_Command(void);
static int State_CloseError(void);
static int State_Open_Command(void);
static int State_Door_Opening(void);
static int State_Idle(void);
static int State_OpenError(void);

int (*state)(void) = State_WatchDoor;

/// Tasks...
static void LedTask(void);
static void UartTask(void);
static void SensorTask(void);
static void ButtonTask(void);

/// Commands & statuses
bool RxCommand_open;
bool RxCommand_close;
bool Sensor_closed;
bool Sensor_open;
bool Lockdown;
bool NewState;

void Event_ButtonPressedShort(void);
void Event_ButtonPressedLong(void);

// other functions...
void setup(void);
void EnterStateMachine(void);
bool FirstTime(void);

void setup()
{
    // GPIOs
    LED_OFF();
    PD_DDR  |= (BLUE_LED_PIN | RED_LED_PIN);
    PC_DDR  |= DOOR_SWITCH_PIN;
    PC_CR1  |= DOOR_SWITCH_PIN;

    TimersSetup();
    INTERRUPT_EN();    
}

void main()
{
    setup();

    RED_LED_ON();
    SetNotification(0);
    while (!IsTimePassed());
    RED_LED_OFF();

    EnterStateMachine();
}

void EnterStateMachine()
{
    int next;
    NewState = true;

    for (;;)
    {
        next = state();
        LedTask();
        UartTask();
        SensorTask();
        ButtonTask();
        if (next != (int)state)
        {
            NewState = true;
        }
        state = (int(*)(void))next;
    }
}

bool FirstTime(void)
{
    if (NewState)
    {
        NewState = false;
        return true;
    }
    return false;
}

//////////////////////////////////////////////////////////////////////////
////////      STATE MACHINE LOGIC  ///////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

//#define GARAGE_DOOR_CLOSING_TIME 10
//#define GARAGE_DOOR_LET_OPEN_TIME 120
#define GARAGE_DOOR_CLOSING_TIME 3
#define GARAGE_DOOR_LET_OPEN_TIME 5

#define RELAY_TIME_CLOSE 1

#define CLOSE_RETIRES_MAX       3
static int close_retries;

int State_WatchDoor()
{
    if (FirstTime())
    {
        RxCommand_open = false;
    }

    if (RxCommand_open && !Lockdown)
    {
        return (int)State_Open_Command;
    }
    if (Sensor_open)
    {
        return (int)State_Wait2Minutes;
    }
    return (int)State_WatchDoor;
}

int State_Open_Command()
{
    if (FirstTime())
    {
        RELAY_CLOSE();
        SetNotification(RELAY_TIME_CLOSE);
    }

    if (IsTimePassed())
    {
        RELAY_OPEN();
        return (int)State_Door_Opening;
    }
    return (int)State_Open_Command;
}

static int State_Door_Opening(void)
{
    if (FirstTime())
    {
        SetNotification(GARAGE_DOOR_CLOSING_TIME);
    }

    if (IsTimePassed())
    {
        return (int)State_OpenError;
    }
    if (Sensor_open)
    {
        return (int)State_Idle;
    }
    return (int)State_Open_Command;
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
        close_retries = CLOSE_RETIRES_MAX;
        return (int)State_Close_Command;
    }
    if (Sensor_closed)
    {
        return (int)State_WatchDoor;
    }
    return (int)State_Idle;
}

int State_Wait2Minutes()
{
    if (FirstTime())
    {
        SetNotification(GARAGE_DOOR_LET_OPEN_TIME);
    }

    if (Sensor_closed)
    {
        return (int)State_WatchDoor;
    }
    if (IsTimePassed())
    {
        close_retries = CLOSE_RETIRES_MAX;
        return (int)State_Close_Command;
    }
    return (int)State_Wait2Minutes;
}

int State_Close_Command()
{
    if (FirstTime())
    {
        RELAY_CLOSE();
        SetNotification(RELAY_TIME_CLOSE);
    }

    if (IsTimePassed())
    {
        RELAY_OPEN();
        return (int)State_Door_Closing;
    }
    return (int)State_Close_Command;
}

int State_Door_Closing()
{
    if (FirstTime())
    {
        SetNotification(GARAGE_DOOR_CLOSING_TIME);
    }

    if (Sensor_closed)
    {
        return (int)State_WatchDoor;
    }
    if (IsTimePassed())
    {
        if (close_retries > 0)
        {
            close_retries--;
            return (int)State_Close_Command;
        }
        else
        {
            return (int)State_CloseError;
        }
        
    }
    return (int)State_Door_Closing;
}

int State_CloseError()
{
    if (Sensor_closed)
    {
        return (int)State_WatchDoor;
    }

    return (int)State_CloseError;
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define BLINK_FREQ_MS 200

void BLUE_LED_BLINK(void)
{
    if ((get_milliseconds_now() % BLINK_FREQ_MS) > (BLINK_FREQ_MS/2))
    {
        BLUE_LED_ON();
    }
    else
    {
        BLUE_LED_OFF();
    }
}

void RED_LED_BLINK(void)
{
    if ((get_milliseconds_now() % BLINK_FREQ_MS) > (BLINK_FREQ_MS/2))
    {
        RED_LED_ON();
    }
    else
    {
        RED_LED_OFF();
    }
}

void PINK_LED_ON(void)
{
    char t = get_milliseconds_now() % 3;
    if (t == 0)
    {
        RED_LED_ON();
        BLUE_LED_OFF();
    }
#if 0		
    else if (t == 1)
    {
        RED_LED_OFF();
        BLUE_LED_OFF();
    }
#endif
    else
    {
        RED_LED_OFF();
        BLUE_LED_ON();
    }
}

void LedTask(void)
{
    if (state == State_WatchDoor)
    {
        BLUE_LED_ON();
        if (Lockdown)
        {
            RED_LED_BLINK();
        }
        else
        {
            RED_LED_OFF();
        }
    }
    else if (state == State_Wait2Minutes)
    {
        BLUE_LED_OFF();
        RED_LED_ON();
    }
    else if (state == State_Door_Closing)
    {
        BLUE_LED_BLINK();
    }
    else if (state == State_CloseError)
    {
        PINK_LED_ON();
    }    
    else
    {
        LED_OFF();
    }
}

void UartTask(void)
{
}

void SensorTask(void)
{
    // TODO: Debounce plz
    if (GET_SENSOR()) // == 1 when open
    {
        Sensor_closed = false;
        Sensor_open = true;       
    }
    else
    {
        Sensor_closed = true;
        Sensor_open = false;
    }
}

void Event_ButtonPressedShort()
{
    Lockdown = !Lockdown;
}

void Event_ButtonPressedLong()
{
    
}

void ButtonTask(void)
{
    #define DEBOUNCE_DELAY_MS 20
    static int lastDebounceTime = 0;
    static char last_button_temp_status = 0;
    static char last_button_status;

    enum
    {
        BUTTON_DOWN = 0,
        BUTTON_UP = BUTTON_PIN
    };

    // read the state of the switch into a local variable:
    char reading = GET_BUTTON();

    // check to see if you just pressed the button
    // (i.e. the input went from LOW to HIGH), and you've waited long enough
    // since the last press to ignore any noise:

    // If the switch changed, due to noise or pressing:
    if (reading != last_button_temp_status) {
    // reset the debouncing timer
        lastDebounceTime = get_milliseconds_now();
        // save the reading. Next time through the loop, it'll be the lastButtonState:
        last_button_temp_status = reading;        
    }

    if (get_milliseconds_since(lastDebounceTime) > DEBOUNCE_DELAY_MS) {
        // whatever the reading is at, it's been there for longer than the debounce
        // delay, so take it as the actual current state:

        // if the button state has changed:
        if (reading != last_button_status) {
            last_button_status = reading;

            // only toggle the LED if the new button state is HIGH
            if (last_button_status == BUTTON_DOWN) {
                Event_ButtonPressedShort();
            }
        }
    }
}
