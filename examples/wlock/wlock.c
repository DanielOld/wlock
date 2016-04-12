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
#include "nrf_sdm.h"
#include "ble.h"
#include "ble_hci.h"
#include "ble_db_discovery.h"
#include "softdevice_handler.h"
#include "app_util.h"
#include "app_error.h"
#include "boards.h"
#include "nrf_gpio.h"
#include "pstorage.h"
#include "device_manager.h"
#include "app_trace.h"
#include "ble_hrs_c.h"
#include "ble_bas_c.h"
#include "app_util.h"
#include "app_timer.h"
#include "bsp.h"
#include "bsp_btn_ble.h"
#include "nrf_drv_gpiote.h"
#include "nrf_delay.h"

#ifdef __SUPPORT_WLOCK__
#include "wlock.h"

wlock_data_t m_wlock_data;
APP_TIMER_DEF(m_wlock_sec_timer_id);  


extern void scan_start(void);
extern void sleep_mode_enter(void);

static void wlock_gpio_set(nrf_drv_gpiote_pin_t pin, bool state)
{
    if(state) {
        NRF_GPIO->OUTSET = (1UL << pin);
    }else {
	    NRF_GPIO->OUTCLR = (1UL << pin);
    }
}

static bool wlock_gpio_get(nrf_drv_gpiote_pin_t pin)
{
    return (NRF_GPIO->IN >> pin) & 0x1UL;
}


static void wlock_voice_aware(void)
{
    int i,j,k;

	for(i=0; i<2; i++)
	{
	    for(j=0; j<250; j++)
	    {
	        wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_ON);
        	nrf_delay_us(100);
	        wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_OFF);
        	nrf_delay_us(200);
	    }
		nrf_delay_ms(500);
	}
}

static void wlock_voice_warning(void)
{
    int i,j,k,m;

	for(m=0; m<60; m++)
	{
	    for(k=10; k<60; k++)
	    {
	        for (j=0;j<20;j++)
	        {
	            wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_ON);
       	        nrf_delay_us(150);
	            wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_OFF);
       	        for(i=0;i<k;i++)
       	        {
       	            nrf_delay_us(5);
       	        }
       	    }	
	    }
	}
}

static void wlock_gsm_power_on(wlock_power_on_cause_t cause)
{
    m_wlock_data.gsm_power_key_interval = WLOCK_GSM_POWER_KEY_INTERVAL;
    wlock_gpio_set(GPIO_GSM_POWER_KEY, BOOL_GSM_PWRKEY_ON);
    wlock_gpio_set(GPIO_GSM_POWER_ON, BOOL_GSM_PWRON_ON);
    switch(cause) 
    {
        case WLOCK_GSM_POWER_ON_WARNING:
		break;
		case WLOCK_GSM_POWER_ON_LVD:
	    wlock_gpio_set(GPIO_GSM_LOW_POWER_INDICATE, BOOL_GSM_LVD_ON);
		break;
    }
}

static void wlock_gpio_event_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    switch(pin) {
		case GPIO_CHARGE_STATE:
			if(wlock_gpio_get(GPIO_CHARGE_STATE) == BOOL_IS_CHR)
			{
				m_wlock_data.in_charge_flag = true;
			} else {
				m_wlock_data.in_charge_flag = false;
			}
			break;
		case GPIO_INFRARED_TRIGGER:
			if (m_wlock_data.aware_flag == false) {
				m_wlock_data.aware_flag = true;
				m_wlock_data.warning_filter++;
			} 
			else if(m_wlock_data.warning_filter >= WLOCK_WARNING_FILTER_COUNT)
			{
				m_wlock_data.warning_flag = true;
			}
			else
			{
			    m_wlock_data.warning_filter++;
			}
			break;
		case GPIO_VIBRATE_TRIGGER:
			if (m_wlock_data.aware_flag == false) {
				m_wlock_data.aware_flag = true;
				m_wlock_data.warning_filter++;
			} 
			else if(m_wlock_data.warning_filter >= WLOCK_WARNING_FILTER_COUNT)
			{
				m_wlock_data.warning_flag = true;
			}
			else
			{
			    m_wlock_data.warning_filter++;
			}
			break;
  		case GPIO_LOW_VOLTAGE_DETECT:
			m_wlock_data.lvd_flag = true;
			break;
		default:
			break;
    	}
}

static void wlock_reset_parameters(void)
{
    /*leave lvd_flag alone */
	m_wlock_data.lvd_warning_interval = 0;
	/* leave lvd_rewarning_interval alone*/
	m_wlock_data.gsm_power_key_interval = 0;
    m_wlock_data.in_charge_flag = false;
	m_wlock_data.aware_flag = false;
	m_wlock_data.aware_interval = 0;
    m_wlock_data.ble_connected_flag = false;
	m_wlock_data.ble_disconnected_flag = false;
	m_wlock_data.ble_scan_timeout_flag = false;
    m_wlock_data.warning_flag = false; 
	m_wlock_data.warning_filter = 0;
	m_wlock_data.warning_interval = 0;
    nrf_drv_gpiote_in_event_enable(GPIO_INFRARED_TRIGGER, true);
    nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, true);
}
static void wlock_sec_timer_handler(void * p_context)
{
    switch(m_wlock_data.wlock_state)
    {
        case WLOCK_STATE_IDLE:
			if(m_wlock_data.aware_flag)
			{  
			    /* enter aware state */
				m_wlock_data.aware_interval = WLOCK_AWARE_INTERVAL;
				m_wlock_data.wlock_state = WLOCK_STATE_AWARE;
			    wlock_voice_aware();
			} else if(m_wlock_data.lvd_flag)
			{
			    if(m_wlock_data.in_charge_flag == true)
			    {
			        m_wlock_data.lvd_flag = false;
					m_wlock_data.lvd_rewarning_interval = 0;
			    } else if(m_wlock_data.lvd_rewarning_interval <= 0)
			    {
				    nrf_drv_gpiote_in_event_disable(GPIO_INFRARED_TRIGGER);
				    nrf_drv_gpiote_in_event_disable(GPIO_VIBRATE_TRIGGER);
				    wlock_gsm_power_on(WLOCK_GSM_POWER_ON_LVD);
					m_wlock_data.lvd_warning_interval = WLOCK_LVD_WARNING_INTERVAL; 
					m_wlock_data.lvd_rewarning_interval = WLOCK_LVD_REWARNING_INTERVAL;
				    m_wlock_data.wlock_state = WLOCK_STATE_LVD;
			    }
				else
				{
				    m_wlock_data.lvd_rewarning_interval--;
				}
			}
			else
			{
			    /* go to sleep */
			    nrf_drv_gpiote_in_event_enable(GPIO_INFRARED_TRIGGER, true);
			    nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, true);
			    nrf_drv_gpiote_in_event_enable(GPIO_LOW_VOLTAGE_DETECT, true);
				sleep_mode_enter();
			}
        break;
		case WLOCK_STATE_AWARE:
			if(m_wlock_data.warning_flag)
			{
			    /* enter ble scanning state */
		        nrf_drv_gpiote_in_event_disable(GPIO_INFRARED_TRIGGER);
		        nrf_drv_gpiote_in_event_disable(GPIO_VIBRATE_TRIGGER);
				scan_start();
				m_wlock_data.wlock_state = WLOCK_STATE_BLE_SCANNING;
			}
			else if(m_wlock_data.aware_interval > 0)
			{
			    m_wlock_data.aware_interval--;
			}
			else
			{
				wlock_reset_parameters();
			    m_wlock_data.wlock_state = WLOCK_STATE_IDLE;
			}
		break;
		case WLOCK_STATE_BLE_SCANNING:
			if(m_wlock_data.ble_connected_flag)
			{
			    m_wlock_data.wlock_state = WLOCK_STATE_BLE_CONNECTED;
			}
			else if(m_wlock_data.ble_scan_timeout_flag)
			{
			    wlock_gsm_power_on(WLOCK_GSM_POWER_ON_WARNING);
				m_wlock_data.warning_interval = WLOCK_WARNING_INTERVAL;
			    m_wlock_data.wlock_state = WLOCK_STATE_WARNING;
			    wlock_voice_warning();
			}
		break;
		case WLOCK_STATE_BLE_CONNECTED:
			if (m_wlock_data.ble_disconnected_flag)
			{
			    m_wlock_data.wlock_state = WLOCK_STATE_BLE_DISCONNECTED;
			}
    	    wlock_gpio_set(GPIO_LED1, BOOL_LED_ON);
		break;
		case WLOCK_STATE_BLE_DISCONNECTED:
    	    wlock_gpio_set(GPIO_LED1, BOOL_LED_OFF);
			wlock_reset_parameters();
			m_wlock_data.wlock_state = WLOCK_STATE_IDLE;
		break;
		case WLOCK_STATE_WARNING:
			if(m_wlock_data.warning_interval > 0)
			{
			    m_wlock_data.warning_interval--;
				if (m_wlock_data.gsm_power_key_interval > 0)
				{
				    m_wlock_data.gsm_power_key_interval--;
				}
				else if (m_wlock_data.gsm_power_key_interval == 0)
				{
				    wlock_gpio_set(GPIO_GSM_POWER_KEY, BOOL_GSM_PWRKEY_OFF);
				    wlock_gpio_set(GPIO_GSM_POWER_ON, BOOL_GSM_PWRON_OFF);
				}
			}
			else
			{
				wlock_reset_parameters();
			    m_wlock_data.wlock_state = WLOCK_STATE_IDLE;
			}
		break;
		case WLOCK_STATE_LVD:
			if(m_wlock_data.lvd_warning_interval > 0)
			{
			    m_wlock_data.lvd_warning_interval--;
				if (m_wlock_data.gsm_power_key_interval > 0)
				{
				    m_wlock_data.gsm_power_key_interval--;
				}
				else if (m_wlock_data.gsm_power_key_interval == 0)
				{
				    wlock_gpio_set(GPIO_GSM_POWER_KEY, BOOL_GSM_PWRKEY_OFF);
				    wlock_gpio_set(GPIO_GSM_POWER_ON, BOOL_GSM_PWRON_OFF);
				}
			}
			else
			{
		   	    wlock_gpio_set(GPIO_GSM_LOW_POWER_INDICATE, BOOL_GSM_LVD_OFF);
				wlock_reset_parameters();
			    m_wlock_data.wlock_state = WLOCK_STATE_IDLE;
			}		
		break;
    }
}

uint32_t wlock_init(void)
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

    /* charge state */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_TOGGLE;
    config.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(GPIO_CHARGE_STATE, &config, wlock_gpio_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    /* vibrate trigger */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(GPIO_VIBRATE_TRIGGER, &config, wlock_gpio_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    /* vibrate trigger */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(GPIO_INFRARED_TRIGGER, &config, wlock_gpio_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    /* low power detect */
    config.is_watcher = false;
	config.hi_accuracy = false;
	if(BOOL_IS_LVD)
	{
	    config.sense = NRF_GPIOTE_POLARITY_LOTOHI;
	} else
	{
	    config.sense = NRF_GPIOTE_POLARITY_HITOLO;
	}
    config.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(GPIO_LOW_VOLTAGE_DETECT, &config, wlock_gpio_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    /* LED1 */
    NRF_GPIO->PIN_CNF[GPIO_LED1] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_LED1, BOOL_LED_OFF);
	

    /* voice */
    NRF_GPIO->PIN_CNF[GPIO_SPERAKER] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_OFF);


    /* infrared power on */
    NRF_GPIO->PIN_CNF[GPIO_INFRARED_POWER_ON] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_INFRARED_POWER_ON, BOOL_INFRARED_POWER_ON);
	nrf_delay_ms(10);
    
    /* GSM low power indicate */
    NRF_GPIO->PIN_CNF[GPIO_GSM_LOW_POWER_INDICATE] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_GSM_LOW_POWER_INDICATE, BOOL_GSM_LVD_OFF);


    /* GSM power on */
    NRF_GPIO->PIN_CNF[GPIO_GSM_POWER_ON] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_GSM_POWER_ON, BOOL_GSM_PWRON_OFF);


    /* GSM power key */
    NRF_GPIO->PIN_CNF[GPIO_GSM_POWER_KEY] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_GSM_POWER_KEY, BOOL_GSM_PWRKEY_OFF);


    memset(&m_wlock_data, 0, sizeof(wlock_data_t));

    if (wlock_gpio_get(GPIO_CHARGE_STATE) == BOOL_IS_CHR)
    {
        m_wlock_data.in_charge_flag = true;
    }
	if (wlock_gpio_get(GPIO_LOW_VOLTAGE_DETECT) == BOOL_IS_LVD)
	{
	    m_wlock_data.lvd_flag = true;
	}
	m_wlock_data.aware_flag = true; /* set aware flag whatever bootup reason */
	m_wlock_data.wlock_state = WLOCK_STATE_IDLE;
    nrf_drv_gpiote_in_event_enable(GPIO_CHARGE_STATE, true);
    nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, true);
    nrf_drv_gpiote_in_event_enable(GPIO_INFRARED_TRIGGER, true);
    nrf_drv_gpiote_in_event_disable(GPIO_LOW_VOLTAGE_DETECT); /* it will be enabled before sleep */


    err_code =
        app_timer_create(&m_wlock_sec_timer_id, APP_TIMER_MODE_REPEATED, wlock_sec_timer_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = app_timer_start(m_wlock_sec_timer_id, APP_TIMER_TICKS(1000, 0), NULL);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    return err_code;
}

#endif /* __SUPPORT_WLOCK__ */

