/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "nordic_common.h"
#include "app_util.h"
#include "app_error.h"
#include "nrf_gpio.h"
#include "app_trace.h"
#include "nrf_drv_gpiote.h"
#include "nrf_delay.h"
#include "ble_advertising.h"
#ifdef __SUPPORT_WLOCK__
#include "wlock_key.h"

bool m_ble_connected = false;

static void wlock_key_gpio_set(nrf_drv_gpiote_pin_t pin, bool state)
{
    if(state) {
        NRF_GPIO->OUTSET = (1UL << pin);
    }else {
	    NRF_GPIO->OUTCLR = (1UL << pin);
    }
}

static void wlock_key_gpio_event_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    uint32_t err_code;

    switch(pin) {
		case GPIO_KEY:
		if(m_ble_connected == false)
		{
		      err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
    		  APP_ERROR_CHECK(err_code);
    	    //wlock_key_gpio_set(GPIO_LED1, BOOL_LED_ON);
   	    	//nrf_delay_ms(200);
    	    //wlock_key_gpio_set(GPIO_LED1, BOOL_LED_OFF);
		}
		else
		{
    	    wlock_key_gpio_set(GPIO_LED2, BOOL_LED_ON);
   	    	nrf_delay_ms(200);
    	    wlock_key_gpio_set(GPIO_LED2, BOOL_LED_OFF);
		}
		default:
			break;
    	}
}

uint32_t wlock_key_init(void)
{
    uint32_t err_code = NRF_SUCCESS;
	
    nrf_drv_gpiote_in_config_t config;

    if (!nrf_drv_gpiote_is_init())
    {
        err_code = nrf_drv_gpiote_init();
        if (err_code != NRF_SUCCESS)
        {
            return err_code;
        }
    }

    /* key */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(GPIO_KEY, &config, wlock_key_gpio_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_KEY, true);

    /* LED1 */
    NRF_GPIO->PIN_CNF[GPIO_LED1] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    /* LED2 */
    NRF_GPIO->PIN_CNF[GPIO_LED2] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_key_gpio_set(GPIO_LED1, BOOL_LED_ON);
    //wlock_key_gpio_set(GPIO_LED2, BOOL_LED_ON);
   	nrf_delay_ms(200);
    wlock_key_gpio_set(GPIO_LED1, BOOL_LED_OFF);
    //wlock_key_gpio_set(GPIO_LED2, BOOL_LED_OFF);

    return err_code;
}

#endif /* __SUPPORT_WLOCK__ */


