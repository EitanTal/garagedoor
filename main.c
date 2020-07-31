/* MAIN.C file
 * 
 * Copyright (c) 2002-2005 STMicroelectronics
 */

#include <stdint.h>

#include <iostm8s003.h>

#define PORTD_ADDR         ((uint8_t*)(0x500F))
#define PORTD_DATADIR_ADDR ((uint8_t*)(0x5011))


#define BLUE_LED_PIN      (1 << 2)
#define BLUE_LED_PORT      PD_ODR
#define RED_LED_PIN       (1 << 3)
#define RED_LED_PORT       PD_ODR
#define DOOR_SENSOR_PIN   (1 << 6)
#define DOOR_SENSOR_PORT   PC_IDR
#define DOOR_SWITCH_PIN   (1 << 3)
#define DOOR_SWITCH_PORT   PC_ODR


void main()
{
	int i;
	int j;

	PD_DDR |= BLUE_LED_PIN;

	for (;;)
	{
		for (i = 0; i < 255; i++)
		{
			for (j = 0; j < 255; j++)
			{
				if (j < i)
				{
					*PORTD_ADDR |= BLUE_LED_PIN;
				}
				else
				{
					*PORTD_ADDR &= ~BLUE_LED_PIN;
				}
			}
		}
	}
}