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

/* input GPIO */
#define GPIO_CHARGE_STATE 				10
#define GPIO_WARNING_TRIGGER			11
#define GPIO_POWER_LOW_DETECT 			12
#define GPIO_GSM_POWER_STATE			13 /* Get GSM power state */

/* output GPIO */
#define GPIO_LED1						14
#define GPIO_VOICE						15
#define GPIO_INFRARED_POWER_ON 			16 /* Control infrared power */
#define GPIO_GSM_LOW_POWER_IND			17 /* Indicate low power state */
#define GPIO_GSM_POWER_ON				18 /* Open V_BAT */
#define GPIO_GSM_POWER_KEY				19 /* GSM power key */

#define WLOCK_MAX_ENDNODE				50

static wlock_endnode_t g_endnode_mapping[WLOCK_MAX_ENDNODE];

#define EMPTY_ENDNODE_CHAR 0xff

#define ENDNODE_MAPPING_SIZE (sizeof(wlock_endnode_t)*WLOCK_MAX_ENDNODE)

static wlock_endnode_t m_empty_endnode;
static pstorage_handle_t       m_storage_handle;                                      /**< Persistent storage handle for blocks requested by the module. */

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


    memset(g_endnode_mapping, EMPTY_ENDNODE_CHAR, ENDNODE_MAPPING_SIZE);
	  memset(&m_empty_endnode, EMPTY_ENDNODE_CHAR, sizeof(wlock_endnode_t));

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
	    if(memcmp(&g_endnode_mapping[i], &m_empty_endnode, sizeof(wlock_endnode_t)) == 0)
	    {
	        memcpy(&g_endnode_mapping[i], &endnode, sizeof(wlock_endnode_t));
			err_code = wlock_endnode_store();
			break;
	    }
	}
	return err_code;
}


static void wlock_charge_state_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
}

static void wlock_warning_trigger_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
}

static void wlock_power_low_trigger_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
}

static void wlock_gsm_power_state_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
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
	config.sense = NRF_GPIOTE_POLARITY_LOTOHI;
    config.pull = NRF_GPIO_PIN_NOPULL;
    err_code = nrf_drv_gpiote_in_init(GPIO_CHARGE_STATE, &config, wlock_charge_state_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_CHARGE_STATE, true);
    //nrf_drv_gpiote_in_event_disable(GPIO_CHARGE_STATE);



    /* warning trigger */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_LOTOHI;
    config.pull = NRF_GPIO_PIN_NOPULL;
    err_code = nrf_drv_gpiote_in_init(GPIO_WARNING_TRIGGER, &config, wlock_warning_trigger_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_WARNING_TRIGGER, true);
    //nrf_drv_gpiote_in_event_disable(GPIO_WARNING_TRIGGER);

    /* power low detect */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_LOTOHI;
    config.pull = NRF_GPIO_PIN_NOPULL;
    err_code = nrf_drv_gpiote_in_init(GPIO_POWER_LOW_DETECT, &config, wlock_power_low_trigger_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_POWER_LOW_DETECT, true);
    //nrf_drv_gpiote_in_event_disable(GPIO_POWER_LOW_DETECT);
	
    /* GSM power state */
    config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_LOTOHI;
    config.pull = NRF_GPIO_PIN_NOPULL;
    err_code = nrf_drv_gpiote_in_init(GPIO_GSM_POWER_STATE, &config, wlock_gsm_power_state_handler);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }
    nrf_drv_gpiote_in_event_enable(GPIO_GSM_POWER_STATE, true);
    //nrf_drv_gpiote_in_event_disable(GPIO_GSM_POWER_STATE);


    /* LED1 */
    NRF_GPIO->PIN_CNF[GPIO_LED1] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_LED1, 0);
	

    /* voice */
    NRF_GPIO->PIN_CNF[GPIO_VOICE] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_VOICE, 0);


    /* infrared power on */
    NRF_GPIO->PIN_CNF[GPIO_INFRARED_POWER_ON] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_INFRARED_POWER_ON, 0);

    
    /* GSM low power indicate */
    NRF_GPIO->PIN_CNF[GPIO_GSM_LOW_POWER_IND] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos) 
        |(GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)    
        |(GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)  
        |(GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos) 
        |(GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);  
	
    wlock_gpio_set(GPIO_GSM_LOW_POWER_IND, 0);


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

	return err_code;

}


void wlock_voice_warning(unsigned int count)
{
    unsigned char i,j,k,m;

    if (count == 0)
    {
	    for(k=0; k<60; k++)
	    {
	        for(i=10; i<45; i++)
	        {
	            for (m=0;m<20;m++)
	            {
       	            if (!wlock_gpio_get(GPIO_CHARGE_STATE))
       	            {
       	                wlock_gpio_set(GPIO_VOICE, 1);
       	            }
       	            for(j=0;j<20;j++);
       	                wlock_gpio_set(GPIO_VOICE, 0);
       	            for(j=0;j<i;j++);
       	            }	
	        }
	    }
    }
    else
    {
	    for(k=0; k<count; k++)
	    {
	        for(i=0; i<250; i++)
	        {
	            if (!wlock_gpio_get(GPIO_CHARGE_STATE))
	            {
	                wlock_gpio_set(GPIO_VOICE, 1);
	            }
	            for(j=0;j<12;j++);
	            wlock_gpio_set(GPIO_VOICE, 0);
	            for(j=0;j<40;j++);
	        }
	        for(j=0;j<220;j++)
	        {
	            for(i=0; i<200; i++);
	        }
	    }
    }

    for(j=0;j<250;j++)
    {
        for(i=0; i<250; i++);
    }
}


