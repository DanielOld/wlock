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

#include "wlock.h"

static wlock_data_t m_wlock_data;

#define ENDNODE_MAPPING_SIZE (sizeof(wlock_endnode_t)*WLOCK_MAX_ENDNODE)

static wlock_endnode_t g_endnode_mapping[WLOCK_MAX_ENDNODE];
static pstorage_handle_t       m_storage_handle;                                      /**< Persistent storage handle for blocks requested by the module. */


extern void scan_start(void);
extern void sleep_mode_enter(void);

static void pstorage_callback_handler(pstorage_handle_t * p_handle,
                                      uint8_t             op_code,
                                      uint32_t            result,
                                      uint8_t           * p_data,
                                      uint32_t            data_len)
{
}


ret_code_t wlock_endnode_load(void)
{
    pstorage_module_param_t param;
    pstorage_handle_t       block_handle;
    ret_code_t            err_code;

    //DM_MUTEX_LOCK(); /* maybe can be used for spi flash */

    memset(g_endnode_mapping, 0x00, ENDNODE_MAPPING_SIZE);

    param.block_size  = ENDNODE_MAPPING_SIZE;
    param.block_count = 1;
    param.cb          = pstorage_callback_handler;

    err_code = pstorage_register(&param, &m_storage_handle);

    if (err_code == NRF_SUCCESS)
    {
        err_code = pstorage_block_identifier_get(&m_storage_handle, 0, &block_handle);
        if (err_code == NRF_SUCCESS)
        {
            err_code = pstorage_load((uint8_t *)g_endnode_mapping,
                                         &block_handle,
                                         ENDNODE_MAPPING_SIZE,
                                         0);
        }
    }
    return err_code;
}



ret_code_t wlock_endnode_store(void)
{
    pstorage_handle_t       block_handle;
    ret_code_t            err_code;

    err_code = pstorage_block_identifier_get(&m_storage_handle, 0, &block_handle);
    if (err_code == NRF_SUCCESS)
    {
       err_code = pstorage_store(&block_handle,
                            (uint8_t *)g_endnode_mapping,
                            ENDNODE_MAPPING_SIZE,
                            0);
    }
    return err_code;
}

ret_code_t wlock_endnode_clear(void)
{
    pstorage_handle_t       block_handle;
    ret_code_t            err_code;

    err_code = pstorage_block_identifier_get(&m_storage_handle, 0, &block_handle);
    if (err_code == NRF_SUCCESS)
    {

        err_code = pstorage_clear(&block_handle, ENDNODE_MAPPING_SIZE);
    }
		return err_code;
}

bool wlock_endnode_match(wlock_endnode_t endnode)
{
    uint32_t i;

	for(i=0; i<WLOCK_MAX_ENDNODE; i++)
	{
	    if(memcmp(&g_endnode_mapping[i], &endnode, sizeof(wlock_endnode_t)) == 0)
	    {
	        return true;
	    }
	}
    return false;
}

ret_code_t wlock_endnode_add(wlock_endnode_t endnode)
{
    uint32_t i;
    ret_code_t err_code = NRF_ERROR_INTERNAL;

	for(i=0; i<WLOCK_MAX_ENDNODE; i++)
	{
	    if((g_endnode_mapping[i].addr[0] == 0)
		   && (g_endnode_mapping[i].addr[1] == 0)
		   && (g_endnode_mapping[i].addr[2] == 0)
		   && (g_endnode_mapping[i].addr[3] == 0)
		   && (g_endnode_mapping[i].addr[4] == 0)
		   && (g_endnode_mapping[i].addr[5] == 0))
	    {
	        memcpy(&g_endnode_mapping[i], &endnode, sizeof(wlock_endnode_t));
			err_code = wlock_endnode_store();
			break;
	    }
	}
	return err_code;
}

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
}
static void wlock_voice_warning(void)
{

}

static void wlock_gsm_power_on(wlock_power_on_cause_t cause)
{
    m_wlock_data.gsm_power_key_interval = WLOCK_GSM_POWER_KEY_INTERVAL;
    wlock_gpio_set(GPIO_GSM_POWER_KEY, BOOL_PWRKEY_ON);
    wlock_gpio_set(GPIO_GSM_POWER_ON, BOOL_PWRON_ON);
    switch(cause) 
    {
        case WLOCK_GSM_POWER_ON_WARNING:
		break;
		case WLOCK_GSM_POWER_ON_LVD:
	    wlock_gpio_set(GPIO_GSM_LOW_POWER_INDICATE, BOOL_LVD_ON);
		break;
    }
}

static void wlock_gpio_event_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    switch(pin) {
		case GPIO_CHARGE_STATE:
			if(wlock_gpio_get(GPIO_CHARGE_STATE) == BOOL_CHR_ON)
			{
				m_wlock_data.in_charge_flag = true;
			} else {
				m_wlock_data.in_charge_flag = false;
			}
			break;
		case GPIO_INFRARED_TRIGGER:
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
/*			
		case GPIO_GSENSOR_TRIGGER:
		break;
*/
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
			    wlock_voice_aware();
				m_wlock_data.wlock_state = WLOCK_STATE_AWARE;
			} else if(m_wlock_data.lvd_flag)
			{
		        nrf_drv_gpiote_in_event_disable(GPIO_LOW_VOLTAGE_DETECT);
			    if(m_wlock_data.in_charge_flag == true)
			    {
			        m_wlock_data.lvd_flag = false;
					m_wlock_data.lvd_rewarning_interval = 0;
			    } else if(m_wlock_data.lvd_rewarning_interval <= 0)
			    {
				    wlock_gsm_power_on(WLOCK_GSM_POWER_ON_LVD);
					m_wlock_data.lvd_warning_interval = WLOCK_LVD_WARNING_INTERVAL; 
					m_wlock_data.lvd_rewarning_interval = WLOCK_LVD_REWARNING_INTERVAL;
				    nrf_drv_gpiote_in_event_disable(GPIO_INFRARED_TRIGGER);
				    nrf_drv_gpiote_in_event_disable(GPIO_VIBRATE_TRIGGER);
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
			else if(m_wlock_data.aware_interval <= 0)
			{
			    m_wlock_data.wlock_state = WLOCK_STATE_IDLE;
			}
			else
			{
			    m_wlock_data.aware_interval--;
			}
		break;
		case WLOCK_STATE_BLE_SCANNING:
			if(m_wlock_data.ble_connected_flag)
			{
			    m_wlock_data.wlock_state = WLOCK_STATE_BLE_CONNECTED;
			}
			else if(m_wlock_data.ble_scan_timeout_flag)
			{
			    wlock_voice_warning();
			    wlock_gsm_power_on(WLOCK_GSM_POWER_ON_WARNING);
				m_wlock_data.warning_interval = WLOCK_WARNING_INTERVAL;
			    m_wlock_data.wlock_state = WLOCK_STATE_WARNING;
			}
		break;
		case WLOCK_STATE_BLE_CONNECTED:
			if (m_wlock_data.ble_disconnected_flag)
			{
			    m_wlock_data.wlock_state = WLOCK_STATE_BLE_DISCONNECTED;
			}
    	    wlock_gpio_set(GPIO_LED1, 1);
		break;
		case WLOCK_STATE_BLE_DISCONNECTED:
    	    wlock_gpio_set(GPIO_LED1, 0);
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
				    wlock_gpio_set(GPIO_GSM_POWER_KEY, BOOL_PWRKEY_OFF);
				    wlock_gpio_set(GPIO_GSM_POWER_ON, BOOL_PWRON_OFF);
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
				    wlock_gpio_set(GPIO_GSM_POWER_KEY, BOOL_PWRKEY_OFF);
				    wlock_gpio_set(GPIO_GSM_POWER_ON, BOOL_PWRON_OFF);
				}
			}
			else
			{
		   	    wlock_gpio_set(GPIO_GSM_LOW_POWER_INDICATE, BOOL_LVD_OFF);
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

	wlock_endnode_load();
		
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
    config.pull = NRF_GPIO_PIN_NOPULL;
    err_code = nrf_drv_gpiote_in_init(GPIO_CHARGE_STATE, &config, wlock_gpio_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_CHARGE_STATE, true);

    /* vibrate trigger */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_NOPULL;
    err_code = nrf_drv_gpiote_in_init(GPIO_VIBRATE_TRIGGER, &config, wlock_gpio_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, true);

    /* vibrate trigger */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_NOPULL;
    err_code = nrf_drv_gpiote_in_init(GPIO_INFRARED_TRIGGER, &config, wlock_gpio_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_INFRARED_TRIGGER, true);

    /* low power detect */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_LOTOHI;
    config.pull = NRF_GPIO_PIN_NOPULL;
    err_code = nrf_drv_gpiote_in_init(GPIO_LOW_VOLTAGE_DETECT, &config, wlock_gpio_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_LOW_VOLTAGE_DETECT, true);
	

    /* LED1 */
    NRF_GPIO->PIN_CNF[GPIO_LED1] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_LED1, 0);
	

    /* voice */
    NRF_GPIO->PIN_CNF[GPIO_SPERAKER] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_SPERAKER, 0);


    /* infrared power on */
    NRF_GPIO->PIN_CNF[GPIO_INFRARED_POWER_ON] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_INFRARED_POWER_ON, 1);

    
    /* GSM low power indicate */
    NRF_GPIO->PIN_CNF[GPIO_GSM_LOW_POWER_INDICATE] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_GSM_LOW_POWER_INDICATE, 0);


    /* GSM power on */
    NRF_GPIO->PIN_CNF[GPIO_GSM_POWER_ON] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_GSM_POWER_ON, 0);


    /* GSM power key */
    NRF_GPIO->PIN_CNF[GPIO_GSM_POWER_KEY] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_GSM_POWER_KEY, 0);

    err_code =
        app_timer_create(&m_wlock_data.sec_timer_id, APP_TIMER_MODE_REPEATED, wlock_sec_timer_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = app_timer_start(m_wlock_data.sec_timer_id, APP_TIMER_TICKS(1000, 0), NULL);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    return err_code;
}




