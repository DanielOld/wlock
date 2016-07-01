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
#include "app_error.h"
#include "nrf_gpio.h"
#include "pstorage.h"
#include "app_trace.h"
#include "app_timer.h"
#include "nrf_drv_gpiote.h"
#include "nrf_delay.h"
#include "ble_advertising.h"
#include "nrf_sdm.h"


#ifdef __SUPPORT_WLOCK__
#include "wlock.h"
#include "kxcjk1013.h"


extern bool twi_master_init(void);


wlock_data_t m_wlock_data;
APP_TIMER_DEF(m_wlock_sec_timer_id);  

#define ENDNODE_MAPPING_SIZE (sizeof(wlock_endnode_t)*WLOCK_MAX_ENDNODE)

static wlock_endnode_t g_endnode_mapping[WLOCK_MAX_ENDNODE];
static pstorage_handle_t       m_storage_handle;   

extern void scan_start(void);
//extern ret_code_t device_instance_find(ble_gap_addr_t const * p_addr, uint32_t * p_device_index);


static void wlock_sleep_mode_enter(void)
{
	uint32_t err_code;
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}


static void wlock_pstorage_callback_handler(pstorage_handle_t * p_handle,
                                      uint8_t             op_code,
                                      uint32_t            result,
                                      uint8_t           * p_data,
                                      uint32_t            data_len)
{
    APP_ERROR_CHECK(result);
}


static bool wlock_endnode_load(void)
{
    pstorage_module_param_t param;
    pstorage_handle_t       block_handle;
    ret_code_t            err_code;

    //DM_MUTEX_LOCK(); /* maybe can be used for spi flash */
    err_code = pstorage_init();
    APP_ERROR_CHECK(err_code);

    memset(g_endnode_mapping, 0x00, ENDNODE_MAPPING_SIZE);

    param.block_size  = ENDNODE_MAPPING_SIZE;
    param.block_count = 1;
    param.cb          = wlock_pstorage_callback_handler;

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
    if(err_code == NRF_SUCCESS)
	{
	    return true;
	}
	else
	{
	    return false;
	}
}



static bool wlock_endnode_store(void)
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
    if(err_code == NRF_SUCCESS)
	{
	    return true;
	}
	else
	{
	    return false;
	}
}

bool wlock_endnode_clear(void)
{
    pstorage_handle_t       block_handle;
    ret_code_t            err_code;

    err_code = pstorage_block_identifier_get(&m_storage_handle, 0, &block_handle);
    if (err_code == NRF_SUCCESS)
    {

        err_code = pstorage_clear(&block_handle, ENDNODE_MAPPING_SIZE);
    }

    if(err_code == NRF_SUCCESS)
	{
	    return true;
	}
	else
	{
	    return false;
	}
}

static bool wlock_endnode_match(wlock_endnode_t endnode)
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

static bool wlock_endnode_add(wlock_endnode_t endnode)
{
    uint32_t i;
    ret_code_t err_code = NRF_ERROR_INTERNAL;

	for(i=0; i<WLOCK_MAX_ENDNODE; i++)
	{
	    if((g_endnode_mapping[i].addr[0] == 0xff)
		   && (g_endnode_mapping[i].addr[1] == 0xff)
		   && (g_endnode_mapping[i].addr[2] == 0xff)
		   && (g_endnode_mapping[i].addr[3] == 0xff)
		   && (g_endnode_mapping[i].addr[4] == 0xff)
		   && (g_endnode_mapping[i].addr[5] == 0xff))
	    {
	        memcpy(&g_endnode_mapping[i], &endnode, sizeof(wlock_endnode_t));
			err_code = wlock_endnode_store();
			break;
	    }
	}

    if(err_code == NRF_SUCCESS)
	{
	    return true;
	}
	else
	{
	    return false;
	}
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

/*
static void wlock_voice_connected(void)
{
    int i,j;

	for(i=0; i<1; i++)
	{
	    for(j=0; j<250; j++)
	    {
	        wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_ON);
        	nrf_delay_us(100);
	        wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_OFF);
        	nrf_delay_us(100);
	    }
		//nrf_delay_ms(500);
	}
}
*/

static void wlock_voice_aware(void)
{
    int i,j;

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
		case GPIO_GSENSOR_INT:
			   kxcjk1013_interrupt_release();
		case GPIO_INFRARED_TRIGGER:
		case GPIO_VIBRATE_TRIGGER:
		case GPIO_LOCK_PICKING:
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
    //m_wlock_data.in_charge_flag = false;
	m_wlock_data.aware_flag = false;
	m_wlock_data.aware_interval = 0;
    m_wlock_data.ble_c_connected_flag = false;
	m_wlock_data.ble_p_connected_flag = false;
    m_wlock_data.warning_flag = false; 
	m_wlock_data.warning_filter = 0;
	m_wlock_data.warning_interval = 0;
    nrf_drv_gpiote_in_event_enable(GPIO_INFRARED_TRIGGER, true);
    nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, true);
    nrf_drv_gpiote_in_event_enable(GPIO_LOCK_PICKING, true);
    nrf_drv_gpiote_in_event_enable(GPIO_GSENSOR_INT, true);
}
static void wlock_sec_timer_handler(void * p_context)
{
    static bool led_on = false;
    ret_code_t err_code;
	static int ble_connect_timeout = 0;

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
				    nrf_drv_gpiote_in_event_disable(GPIO_LOCK_PICKING);
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
			    /* check lvd again before entering sleep */
				if (wlock_gpio_get(GPIO_LOW_VOLTAGE_DETECT) == BOOL_IS_LVD) 
				{
	    			m_wlock_data.lvd_flag = true;
					m_wlock_data.lvd_rewarning_interval = 0;
				}
				else
				{
			    	/* go to sleep */
				    nrf_drv_gpiote_in_event_enable(GPIO_INFRARED_TRIGGER, true);
				    nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, true);
				    nrf_drv_gpiote_in_event_enable(GPIO_LOCK_PICKING, true);
			    	nrf_drv_gpiote_in_event_enable(GPIO_LOW_VOLTAGE_DETECT, true);
			    	nrf_drv_gpiote_in_event_enable(GPIO_GSENSOR_INT, true);
					wlock_sleep_mode_enter();
				}
			}
        break;
		case WLOCK_STATE_AWARE:
			if(m_wlock_data.warning_flag)
			{
			    /* enter ble scanning state */
		        nrf_drv_gpiote_in_event_disable(GPIO_INFRARED_TRIGGER);
		        nrf_drv_gpiote_in_event_disable(GPIO_VIBRATE_TRIGGER);
		        nrf_drv_gpiote_in_event_disable(GPIO_LOCK_PICKING);
		        nrf_drv_gpiote_in_event_disable(GPIO_GSENSOR_INT);
				ble_connect_timeout = 0;
				scan_start();
                err_code = ble_advertising_start(BLE_ADV_MODE_FAST);
                APP_ERROR_CHECK(err_code);
				m_wlock_data.wlock_state = WLOCK_STATE_BLE_CONNECTING;
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
		case WLOCK_STATE_BLE_CONNECTING:
			if((m_wlock_data.ble_c_connected_flag == true) || 
				(m_wlock_data.ble_p_connected_flag == true))
			{
			    m_wlock_data.wlock_state = WLOCK_STATE_BLE_CONNECTED;
#ifdef GPIO_INFRARED_POWER_ON
			    wlock_gpio_set(GPIO_INFRARED_POWER_ON, BOOL_INFRARED_POWER_OFF);
#endif
			}
			else if(ble_connect_timeout >= BLE_WLOCK_TIMEOUT)
			{
			    wlock_gsm_power_on(WLOCK_GSM_POWER_ON_WARNING);
				m_wlock_data.warning_interval = WLOCK_WARNING_INTERVAL;
			    m_wlock_data.wlock_state = WLOCK_STATE_WARNING;
			    wlock_voice_warning();
			} 
			else
			{
			    ble_connect_timeout++;
			}
		break;
		case WLOCK_STATE_BLE_CONNECTED:
			if(led_on == true) { 
				led_on = false;
			} else {
			    wlock_gpio_set(GPIO_LED1, BOOL_LED_ON);
				nrf_delay_ms(10);
			    wlock_gpio_set(GPIO_LED1, BOOL_LED_OFF);
				led_on = true;
			}
			if((m_wlock_data.ble_c_connected_flag == false) && 
				(m_wlock_data.ble_p_connected_flag == false))
			{
			    m_wlock_data.wlock_state = WLOCK_STATE_BLE_DISCONNECTED;
			}
			else if(m_wlock_data.lvd_flag)
			{
			    if(m_wlock_data.in_charge_flag == true)
			    {
			        m_wlock_data.lvd_flag = false;
					m_wlock_data.lvd_rewarning_interval = 0;
			    } else if(m_wlock_data.lvd_rewarning_interval <= 0)
			    {
				    nrf_drv_gpiote_in_event_disable(GPIO_INFRARED_TRIGGER);
				    nrf_drv_gpiote_in_event_disable(GPIO_VIBRATE_TRIGGER);
				    nrf_drv_gpiote_in_event_disable(GPIO_LOCK_PICKING);
				    nrf_drv_gpiote_in_event_disable(GPIO_LOCK_PICKING);
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
		break;
		case WLOCK_STATE_BLE_DISCONNECTED:
		    wlock_gpio_set(GPIO_LED1, BOOL_LED_OFF);
#ifdef GPIO_INFRARED_POWER_ON
			wlock_gpio_set(GPIO_INFRARED_POWER_ON, BOOL_INFRARED_POWER_ON);
			nrf_delay_ms(10);
#endif
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
			if((m_wlock_data.ble_c_connected_flag == true) || 
				(m_wlock_data.ble_p_connected_flag == true))
			{
    			if(led_on == true) { 
    				led_on = false;
    			} 
					else 
					{
    			    wlock_gpio_set(GPIO_LED1, BOOL_LED_ON);
    				nrf_delay_ms(10);
    			    wlock_gpio_set(GPIO_LED1, BOOL_LED_OFF);
    				led_on = true;
    			}
			}
				
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
			if((m_wlock_data.ble_c_connected_flag == true) || 
				(m_wlock_data.ble_p_connected_flag == true))
			    {
			        m_wlock_data.wlock_state = WLOCK_STATE_BLE_CONNECTED;
			    }
				else
				{
				    wlock_reset_parameters();
			        m_wlock_data.wlock_state = WLOCK_STATE_IDLE;
				}
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
    twi_master_init();
	kxcjk1013_init();

#if 1
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
    /* infrared trigger */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(GPIO_INFRARED_TRIGGER, &config, wlock_gpio_event_handler);
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

    /* lock picking trigger */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(GPIO_LOCK_PICKING, &config, wlock_gpio_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    /* lock picking trigger */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_PULLUP;
    err_code = nrf_drv_gpiote_in_init(GPIO_ERASE_KEY, &config, wlock_gpio_event_handler);
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
#endif
    /* gsensor int */
    config.is_watcher = false;
	config.hi_accuracy = false;
    config.sense = NRF_GPIOTE_POLARITY_HITOLO;
    config.pull = NRF_GPIO_PIN_NOPULL;
    err_code = nrf_drv_gpiote_in_init(GPIO_GSENSOR_INT, &config, wlock_gpio_event_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

#ifdef GPIO_LED1
    /* LED1 */
    NRF_GPIO->PIN_CNF[GPIO_LED1] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_LED1, BOOL_LED_OFF);
#endif	

    /* voice */
    NRF_GPIO->PIN_CNF[GPIO_SPERAKER] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_OFF);

#ifdef GPIO_INFRARED_POWER_ON
    /* infrared power on */
    NRF_GPIO->PIN_CNF[GPIO_INFRARED_POWER_ON] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_INFRARED_POWER_ON, BOOL_INFRARED_POWER_ON);
	nrf_delay_ms(10);
#endif
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
#if 1	
    nrf_drv_gpiote_in_event_enable(GPIO_CHARGE_STATE, true);
    nrf_drv_gpiote_in_event_enable(GPIO_INFRARED_TRIGGER, true);
    nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, true);
    nrf_drv_gpiote_in_event_enable(GPIO_LOCK_PICKING, true);
    nrf_drv_gpiote_in_event_disable(GPIO_LOW_VOLTAGE_DETECT); /* it will be enabled before sleep */
#endif
    nrf_drv_gpiote_in_event_enable(GPIO_GSENSOR_INT, true); 

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

bool wlock_is_allowed_to_connect(uint8_t const * addr, int8_t rssi)
{
    bool ret = false;
	wlock_endnode_t node;

	memcpy(node.addr, addr, sizeof(node.addr));
    if(wlock_endnode_match(node) == true)
    {
        ret = true;
    }
	else if (rssi >= BLE_WLOCK_RSSI)
	{
	    ret = wlock_endnode_add(node);
	}
    return ret;
}

#include "ble_hrs.h"
extern ble_hrs_t    m_hrs; 
static uint32_t wlock_ble_tx_handler(uint8_t *data, uint16_t len)
{
    uint32_t err_code;
    uint16_t               hvx_len;
    ble_gatts_hvx_params_t hvx_params;
	
    if (m_hrs.conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        hvx_len = len;
        memset(&hvx_params, 0, sizeof(hvx_params));
        hvx_params.handle   = m_hrs.hrm_handles.value_handle;
        hvx_params.type     = BLE_GATT_HVX_NOTIFICATION;
        hvx_params.offset   = 0;
        hvx_params.p_len    = &hvx_len;
        hvx_params.p_data   = data;
        
        err_code = sd_ble_gatts_hvx(m_hrs.conn_handle, &hvx_params);
        if ((err_code == NRF_SUCCESS) && (hvx_len != len))
        {
            err_code = NRF_ERROR_DATA_SIZE;
        }
    }
    else
    {
        err_code = NRF_ERROR_INVALID_STATE;
    }

    return err_code;

}

/*
header       cmd          len      address                 RSSI     tail
0xaa 0xbb   0x01         0x07    xx xx xx xx xx xx     xx        0xcc 0xdd
00    01        02           03      04 05 06 07 08 09   10        11    12
*/
void wlock_ble_rx_handler(uint8_t *data, uint16_t len)
{
#define PRO_HEAD 0xaabb
#define PRO_TAIL 0xccdd

    uint8_t buf[16];

    if(len == 13)
    {
        if(data[2] == 0x01)
        {
            if(wlock_is_allowed_to_connect(&data[4], (int8_t)data[10]) == true)
            {
    			m_wlock_data.ble_p_connected_flag = true;
                buf[0] = (uint8_t)(PRO_HEAD&0xff);
                buf[1] = (uint8_t)((PRO_HEAD>>8)&0xff);
                buf[2] = data[2];
                buf[3] = 1;
                buf[4] = 0;
                buf[5] = (uint8_t)(PRO_TAIL&0xff);
                buf[6] = (uint8_t)((PRO_TAIL>>8)&0xff);
                wlock_ble_tx_handler(buf, 7);
			}
        }
    }
}


#endif /* __SUPPORT_WLOCK__ */


