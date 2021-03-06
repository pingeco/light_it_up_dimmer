/*
 * The MIT License (MIT)
 *
 * Copyright (c) [2015] [Marco Russi]
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/




/* ------------- Inclusions --------------- */

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "nordic_common.h"
#include "nrf.h"
#include "nrf51_bitfields.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "softdevice_handler.h"
#include "bootloader.h"

#include "config.h"
#include "ble_manager.h"
#include "dimmer_service.h"
#include "led_strip.h"
#include "memory.h"

#include "application.h"




/* ---------------- Local defines --------------------- */

/* Password for starting DFU Upgrade on char write */
#define DFU_UPGRADE_CHAR_PASSWORD				0xA9

/* Default fade percentage value */
#define DEF_FADE_PWM_PERCENT						10		/* 10 % */

/* Number of PWM values groups */
#define NUM_OF_PWM_VALUES_GROUPS					12




/* -------------- Local macros ---------------- */

/* Macro to set spacial value on GPREGRET register to start bootloader after reset */
#define SET_REG_VALUE_TO_START_BOOTLOADER()  		(NRF_POWER->GPREGRET = BOOTLOADER_DFU_START)
 



/* -------------- Local variables ---------------- */

/* Default characteristic values */
const uint8_t default_values[MEM_BUFFER_DATA_LENGTH] = 
{
	DEF_FADE_PWM_PERCENT,		/* Light - Fade */
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF,
	0xFF
};


/* Flag to indicate that adv timeout is elapsed */
static volatile bool adv_timeout = false;


/* Advertising data value and meaning */
/*
	0x10 -> "X UP"			
	0x11 -> "X DOWN"
	0x12 -> "X ROT R"
	0x13 -> "X ROT L"
	0x14 -> "Y UP"
	0x15 -> "Y DOWN"
	0x16 -> "Y ROT R"
	0x17 -> "Y ROT L"
	0x18 -> "Z UP"
	0x19 -> "Z DOWN"
	0x1A -> "Z ROT R"
	0x1B -> "Z ROT L"	
*/
/* PWM values associated to advertising value */
static const uint8_t pwm_values[NUM_OF_PWM_VALUES_GROUPS][4] =
{
/*
	{100, 100, 100, 100},
	{20, 20, 20, 20},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 100, 100},
	{0, 0, 20, 20},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{20, 20, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
	{0, 0, 0, 0},
*/
	{ 10, 10, 10, 10},	/* good night */
	{ 25, 25, 25, 25},	/* low */
	{  0, 0, 0, 0},
	{  0, 0, 0, 0},
	{ 50, 50, 50, 50},	/* mid-low */
	{ 75, 75, 75, 75},	/* mid-high */
	{  0, 0, 0, 0},
	{  0, 0, 0, 0},
	{100, 100, 100, 100},	/* high */
	{  0, 0, 0, 0},	/* OFF */ 
	{  0, 0, 0, 0},
	{  0, 0, 0, 0},
};




/* ---------------- Exported functions --------------------- */   

/* indicate that advertising timeout is elapsed */
void app_on_adv_timeout( void )
{
	/* set related flag */
	adv_timeout = true;
}


/* callback on SPECIAL_OP characteristic write */
void app_on_special_op( uint8_t special_op_byte )
{
	/* if received data is the password for DFU Upgrade */
	if(special_op_byte == DFU_UPGRADE_CHAR_PASSWORD)
	{
		/* set special register value to start bootloader */
		SET_REG_VALUE_TO_START_BOOTLOADER();

		/* perform a system reset */
		NVIC_SystemReset();
	}
	else
	{
		/* do nothing */
	}
}


/* callback on new adv scan */
void application_on_new_scan( uint8_t new_adv_data )
{
	uint8_t pwm_index;
	/* if the upper nibble is as expected */
	if(0x10 == (new_adv_data & 0xF0))
	{
		/* get PWM index */
		pwm_index = (new_adv_data & 0x0F);

		/* check data validity */
		if(pwm_index < NUM_OF_PWM_VALUES_GROUPS)
		{
			/* update RGBW PWM values */
			led_update_light(pwm_values[pwm_index][0],
					 			  pwm_values[pwm_index][1],
					 			  pwm_values[pwm_index][2],
					 			  pwm_values[pwm_index][3]);
		}
		else
		{
			/* invalid data: do nothing */
		}
	}
	else
	{
		/* wrong data: do nothing */
	}
}


/* callback on connection event */
void application_on_conn( void )
{
	/* do nothing at the moment */
}


/* callback on disconnection event */
void application_on_disconn( void )
{
	/* start avertising */
	ble_man_adv_start();
}


/* init application */
void application_init( void )
{
	/* init peripheral connection */
	ble_man_init();

	/* if persistent memory is initialised successfully */
	if(true == memory_init(default_values))
	{
		/* wait for completion */
		while(false != memory_is_busy());
	}
	else
	{
		/* very bad, use default setting as recovery */
	}

	/* init LED module */
	led_light_init();

	/* start avertising */
	ble_man_adv_start();

	/* start scanning */
	ble_man_scan_start();
}


/* main application loop */
void application_run( void )
{
	/* if adv timeout is elapsed */
	if(true == adv_timeout)
	{	
		/* clear flag */
		adv_timeout = false;
#ifdef LED_DEBUG
		//nrf_gpio_pin_write(7, 0);
#endif
		/* start scanning */
		//ble_man_scan_start();
	}
	else
	{
		/* wait for adv timeout */
	}
	
	/* manage light */
	//led_manage_light();
}




/* End of file */






