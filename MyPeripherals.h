#pragma once

#define FCLK_FREQ        2000000

#define BLUE_LED_PIN      (1 << 2)
#define BLUE_LED_PORT      PD_ODR
#define RED_LED_PIN       (1 << 3)
#define RED_LED_PORT       PD_ODR
#define DOOR_SENSOR_PIN   (1 << 6)
#define DOOR_SENSOR_PORT   PC_IDR
#define DOOR_SWITCH_PIN   (1 << 3)
#define DOOR_SWITCH_PORT   PC_ODR
#define BUTTON_PIN        (1 << 4)
#define BUTTON_PORT       PD_IDR


#define BLUE_LED_ON()   BLUE_LED_PORT &= ~BLUE_LED_PIN
#define BLUE_LED_OFF()  BLUE_LED_PORT |=  BLUE_LED_PIN

#define RED_LED_ON()    RED_LED_PORT &= ~RED_LED_PIN
#define RED_LED_OFF()   RED_LED_PORT |=  RED_LED_PIN

#define LED_OFF()       (BLUE_LED_PORT |=  RED_LED_PIN | BLUE_LED_PIN)
#define RELAY_CLOSE()   (DOOR_SWITCH_PORT |=  DOOR_SWITCH_PIN)
#define RELAY_OPEN()    (DOOR_SWITCH_PORT &= ~DOOR_SWITCH_PIN)

#define GET_SENSOR()  (DOOR_SENSOR_PORT & DOOR_SENSOR_PIN)
#define GET_BUTTON()  (BUTTON_PORT & BUTTON_PIN)

#define GET_SENSOR_BOOL()  (!!(GET_SENSOR()))
