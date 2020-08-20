#include <stdint.h>
#include <stdbool.h>
#include <iostm8s003.h>
#include "MyPeripherals.h"
#include "time.h"
#include "tuya.h"

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
    PD_ODR  |= UART_TX_PIN;
    PD_DDR  |= (BLUE_LED_PIN | RED_LED_PIN | UART_TX_PIN);
    PC_DDR  |= DOOR_SWITCH_PIN;
    PC_CR1  |= DOOR_SWITCH_PIN;
    PD_CR1  |= (UART_TX_PIN ); 

    // Others
    TimersSetup();
    UartSetup();
    INTERRUPT_EN();
}

void main()
{
    setup();

    // Test that notification function works:
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
        RxTask();
        TxTask();
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

#define GARAGE_DOOR_CLOSING_TIME    20 /* Actual time: 13.7s */
#define GARAGE_DOOR_LET_OPEN_TIME (120 + GARAGE_DOOR_CLOSING_TIME)

#define RELAY_TIME_CLOSE 1          /* Original firmware uses about 3 */

#define CLOSE_RETIRES_MAX       3
static int close_attempts_remaining;

int State_WatchDoor()
{
    if (FirstTime())
    {
        StatusReport(false,0x65); // 0x65 sends alarm/notification
        StatusReport(false,1);
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
    return (int)State_Door_Opening;
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
    if (FirstTime())
    {
        RxCommand_close = false;
        RxCommand_open = false;
        StatusReport(true,0x65); // 0x65 sends alarm/notification
        StatusReport(true,1);
    }

    if (RxCommand_close)
    {
        close_attempts_remaining = CLOSE_RETIRES_MAX;
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
        RxCommand_close = false;
        StatusReport(true,0x65); // 0x65 sends alarm/notification
        StatusReport(true,1);
        SetNotification(GARAGE_DOOR_LET_OPEN_TIME);
    }

    if (Sensor_closed)
    {
        return (int)State_WatchDoor;
    }
    if (IsTimePassed() || RxCommand_close)
    {
        close_attempts_remaining = CLOSE_RETIRES_MAX;
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
        if ((close_attempts_remaining-1) > 0) // -1 for the one already happened
        {
            close_attempts_remaining--;
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

void RED_LED_BLINK_SLOW(void)
{
    if ((get_milliseconds_now() % BLINK_FREQ_MS) > (BLINK_FREQ_MS*0.9))
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
    if (t == 0) // 33% of the time
    {
        RED_LED_ON();
        BLUE_LED_OFF();
    }
    else // 67% of the time
    {
        RED_LED_OFF();
        BLUE_LED_ON();
    }
}

void PINK_LED_BLINK(void)
{
    char t = get_milliseconds_now() % 100;
    if (t > 50)
    {
        PINK_LED_ON();
    }
    else
    {
        LED_OFF();
    }
}

void LedTask(void)
{
    if (wifiResetInProgress)
    {
        PINK_LED_BLINK();
    }
    else if (state == State_WatchDoor)
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
    else if (state == State_Door_Closing || state == State_Door_Opening)
    {
        RED_LED_OFF();
        BLUE_LED_BLINK();
    }
    else if (state == State_CloseError || state == State_OpenError)
    {
        PINK_LED_ON();
    }
    else if (state == State_Idle)
    {
        BLUE_LED_OFF();
        RED_LED_BLINK_SLOW();
    }
    else
    {
        LED_OFF();
    }
}

void SensorTask(void)
{
    // The sensor doesn't seem to need any debouncing.
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
    static int pairing_mode = 0;
    WifiReset(pairing_mode);
    pairing_mode = !pairing_mode;
}

void ButtonTask(void)
{
    #define DEBOUNCE_DELAY_MS 20
    static int lastDebounceTime = 0;
    static char last_button_temp_status = 0;
    static char last_button_status;
    static uint8_t button_longpress_countdown;
    static int button_longpress_starttime = 0;
    bool isStable;
    bool change;

    enum
    {
        BUTTON_DOWN = 0,
        BUTTON_UP = BUTTON_PIN
    };

    // read the state of the switch into a local variable:
    char reading = GET_BUTTON();

    // If the switch changed, due to noise or pressing:
    if (reading != last_button_temp_status) {
        // reset the debouncing timer
        lastDebounceTime = get_milliseconds_now();
        // save the reading. Next time through the loop, it'll be the lastButtonState:
        last_button_temp_status = reading;
        button_longpress_countdown = 0;
        button_longpress_starttime = get_milliseconds_now();
    }

    // Button is considered stable if it maintained its status for X ms.
    isStable = (get_milliseconds_since(lastDebounceTime) > DEBOUNCE_DELAY_MS);
    if (!isStable) return;

    // determine if the button state has just changed:
    change = false;
    if (reading != last_button_status) {
        last_button_status = reading;
        change = true;
    }

    // Button pressed down event:
    if (change && (last_button_status == BUTTON_DOWN))
    {
        Event_ButtonPressedShort();
    }

    // 
    if (reading == BUTTON_DOWN)
    {
        if (get_milliseconds_since(button_longpress_starttime) > 100)
        {
            button_longpress_starttime = get_milliseconds_now();
            button_longpress_countdown++;
        }

        if ((last_button_status == BUTTON_DOWN) && (button_longpress_countdown > 30))
        {
            button_longpress_countdown = 0;
            Event_ButtonPressedLong();
        }
    }
}
