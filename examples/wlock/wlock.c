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
#include "fds.h"

#ifdef __SUPPORT_WLOCK__
#include "wlock.h"
#include "kxcjk1013.h"


extern bool twi_master_init(void);


wlock_data_t m_wlock_data;
APP_TIMER_DEF(m_wlock_sec_timer_id);
extern void scan_start(void);

/*
static void wlock_sleep_mode_enter(void)
{
	uint32_t err_code;
	err_code = sd_power_system_off();
	APP_ERROR_CHECK(err_code);
}
*/
#if 0
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
	pstorage_handle_t       block_handle;
	ret_code_t            err_code;

	err_code = pstorage_block_identifier_get(&m_storage_handle, 0, &block_handle);
	if (err_code == NRF_SUCCESS)
	{
		err_code = pstorage_load((uint8_t *)g_endnode_mapping,
			&block_handle,
			ENDNODE_MAPPING_SIZE,
			0);
	}
	if (err_code == NRF_SUCCESS)
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
		wlock_endnode_clear();

		err_code = pstorage_store(&block_handle,
			(uint8_t *)g_endnode_mapping,
			ENDNODE_MAPPING_SIZE,
			0);
	}
	if (err_code == NRF_SUCCESS)
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

		//memset(g_endnode_mapping, 0x00, ENDNODE_MAPPING_SIZE);
		err_code = pstorage_clear(&block_handle, ENDNODE_MAPPING_SIZE);
	}

	if (err_code == NRF_SUCCESS)
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

	for (i = 0; i < WLOCK_MAX_ENDNODE; i++)
	{
		if (memcmp(&g_endnode_mapping[i], &endnode, sizeof(wlock_endnode_t)) == 0)
		{
			return true;
		}
	}
	return false;
}

static bool wlock_endnode_add(wlock_endnode_t endnode)
{
	uint32_t i;
	uint32_t j;
	bool ret = false;

	for (i = 0; i < WLOCK_MAX_ENDNODE; i++)
	{
		if ((g_endnode_mapping[i].addr[0] == 0xff)
			&& (g_endnode_mapping[i].addr[1] == 0xff)
			&& (g_endnode_mapping[i].addr[2] == 0xff)
			&& (g_endnode_mapping[i].addr[3] == 0xff)
			&& (g_endnode_mapping[i].addr[4] == 0xff)
			&& (g_endnode_mapping[i].addr[5] == 0xff))
		{
			memcpy(&g_endnode_mapping[i], &endnode, sizeof(wlock_endnode_t));
			ret = wlock_endnode_store();
			break;
		}
	}

	if (i == WLOCK_MAX_ENDNODE) // replace the oldest one
	{
		for (j = 1; j < WLOCK_MAX_ENDNODE; j++)
		{
			memcpy(&g_endnode_mapping[j - 1], &g_endnode_mapping[j], sizeof(wlock_endnode_t));
		}
		memcpy(&g_endnode_mapping[WLOCK_MAX_ENDNODE - 1], &endnode, sizeof(wlock_endnode_t));
		ret = wlock_endnode_store();
	}

	return ret;
}

bool wlock_endnode_init(bool erase)
{
	pstorage_module_param_t param;
	ret_code_t            err_code;
	bool ret = false;

	//DM_MUTEX_LOCK(); /* maybe can be used for spi flash */
	err_code = pstorage_init();
	APP_ERROR_CHECK(err_code);

	memset(g_endnode_mapping, 0xFF, ENDNODE_MAPPING_SIZE);

	param.block_size = ENDNODE_MAPPING_SIZE;
	param.block_count = 1;
	param.cb = wlock_pstorage_callback_handler;

	err_code = pstorage_register(&param, &m_storage_handle);

	if (err_code == NRF_SUCCESS)
	{
		if (erase == true)
		{
			ret = wlock_endnode_clear();
		}
		else
		{
			wlock_endnode_load();
			ret = true;
		}
	}
	return ret;
}
#else
static wlock_endnode_t g_endnode_mapping[WLOCK_MAX_ENDNODE];

#define WLOCK_ENDNODE_INSTANCE_ID 0x3abc
#define WLOCK_ENDNODE_TYPE_ID 0x7abc
#define ENDNODE_MAPPING_SIZE (sizeof(wlock_endnode_t)*WLOCK_MAX_ENDNODE)
#define WLOCK_ENDNODE_DEFAULT_CHAR 0x00
bool m_endnode_completed = false;

static void wlock_endnode_evt_handler(ret_code_t       result,
	fds_cmd_id_t     cmd,
	fds_record_id_t  record_id,
	fds_record_key_t record_key)
{
	if (cmd == FDS_CMD_GC)
	{
		m_endnode_completed = true;
	}
}


static bool wlock_endnode_load(void)
{
	fds_find_token_t find_tok;
	fds_record_desc_t record_desc;
	fds_record_t record;

	fds_find(WLOCK_ENDNODE_TYPE_ID, WLOCK_ENDNODE_INSTANCE_ID, &record_desc, &find_tok);
	fds_open(&record_desc, &record);
	// No need to keep it open, since we are not reading.
	memcpy(g_endnode_mapping, (wlock_endnode_t*)record.p_data, ENDNODE_MAPPING_SIZE);
	fds_close(&record_desc);
	return true;
}



static bool wlock_endnode_store(void)
{
	fds_record_desc_t   record_desc;
	fds_record_key_t    record_key;
	fds_record_chunk_t  chunk;
	fds_find_token_t find_tok;

	if (fds_find(WLOCK_ENDNODE_TYPE_ID, WLOCK_ENDNODE_INSTANCE_ID,
		&record_desc, &find_tok) == NRF_SUCCESS)
	{
		fds_clear(&record_desc);
		fds_gc();
	}
	chunk.length_words = ENDNODE_MAPPING_SIZE / 4;
	chunk.p_data = g_endnode_mapping;
	record_key.instance = WLOCK_ENDNODE_INSTANCE_ID;
	record_key.type = WLOCK_ENDNODE_TYPE_ID;
	// Request write
	fds_write(&record_desc, record_key, 1, &chunk);

	return true;
}

bool wlock_endnode_clear(void)
{
	memset(g_endnode_mapping, WLOCK_ENDNODE_DEFAULT_CHAR, ENDNODE_MAPPING_SIZE);
	wlock_endnode_store();

	return true;
}

static bool wlock_endnode_match(wlock_endnode_t endnode)
{
	uint32_t i;

	for (i = 0; i < WLOCK_MAX_ENDNODE; i++)
	{
		if (memcmp(&g_endnode_mapping[i], &endnode, sizeof(wlock_endnode_t)) == 0)
		{
			return true;
		}
	}
	return false;
}

static bool wlock_endnode_add(wlock_endnode_t endnode)
{
	uint32_t i;
	uint32_t j;
	bool ret = false;

	for (i = 0; i < WLOCK_MAX_ENDNODE; i++)
	{
		if ((g_endnode_mapping[i].addr[0] == WLOCK_ENDNODE_DEFAULT_CHAR)
			&& (g_endnode_mapping[i].addr[1] == WLOCK_ENDNODE_DEFAULT_CHAR)
			&& (g_endnode_mapping[i].addr[2] == WLOCK_ENDNODE_DEFAULT_CHAR)
			&& (g_endnode_mapping[i].addr[3] == WLOCK_ENDNODE_DEFAULT_CHAR)
			&& (g_endnode_mapping[i].addr[4] == WLOCK_ENDNODE_DEFAULT_CHAR)
			&& (g_endnode_mapping[i].addr[5] == WLOCK_ENDNODE_DEFAULT_CHAR))
		{
			memcpy(&g_endnode_mapping[i], &endnode, sizeof(wlock_endnode_t));
			ret = wlock_endnode_store();
			break;
		}
	}

	if (i == WLOCK_MAX_ENDNODE) // replace the oldest one
	{
		for (j = 1; j < WLOCK_MAX_ENDNODE; j++)
		{
			memcpy(&g_endnode_mapping[j - 1], &g_endnode_mapping[j], sizeof(wlock_endnode_t));
		}
		memcpy(&g_endnode_mapping[WLOCK_MAX_ENDNODE - 1], &endnode, sizeof(wlock_endnode_t));
		ret = wlock_endnode_store();
	}

	return ret;
}

bool wlock_endnode_init()
{
	fds_find_token_t find_tok;
	fds_record_desc_t record_desc;
	fds_record_key_t    record_key;
	fds_record_chunk_t  chunk;
	ret_code_t retval;
	bool ret = false;
	fds_cb_t cb;

	cb = wlock_endnode_evt_handler;
	retval = fds_register(cb);
	if (retval != NRF_SUCCESS)
	{
		return false;
	}

	retval = fds_init();
	if (retval != NRF_SUCCESS)
	{
		return false;
	}

	memset(g_endnode_mapping, WLOCK_ENDNODE_DEFAULT_CHAR, ENDNODE_MAPPING_SIZE);


	if (fds_find(WLOCK_ENDNODE_TYPE_ID, WLOCK_ENDNODE_INSTANCE_ID,
		&record_desc, &find_tok) == NRF_SUCCESS)
	{
		ret = wlock_endnode_load();

	}
	else
	{
		chunk.length_words = ENDNODE_MAPPING_SIZE / 4;
		chunk.p_data = g_endnode_mapping;
		record_key.instance = WLOCK_ENDNODE_INSTANCE_ID;
		record_key.type = WLOCK_ENDNODE_TYPE_ID;
		// Request write
		fds_write(&record_desc, record_key, 1, &chunk);
		ret = true;
	}

	return ret;
}
#endif
static void wlock_gpio_set(nrf_drv_gpiote_pin_t pin, bool state)
{
	if (state) {
		NRF_GPIO->OUTSET = (1UL << pin);
	}
	else {
		NRF_GPIO->OUTCLR = (1UL << pin);
	}
}

static bool wlock_gpio_get(nrf_drv_gpiote_pin_t pin)
{
	return (NRF_GPIO->IN >> pin) & 0x1UL;
}

static bool wlock_ble_connected(void)
{
	if (m_wlock_data.ble_c_connected_flag || m_wlock_data.ble_p_connected_flag)
	{
		return true;
	}
	else
	{
		return false;
	}
}

static bool wlock_in_charge_state(void)
{
	return (wlock_gpio_get(GPIO_CHARGE_STATE) == BOOL_IS_CHR);
}

static bool wlock_in_lvd_state(void)
{
	return (wlock_gpio_get(GPIO_LOW_VOLTAGE_DETECT) == BOOL_IS_LVD);
}

static void wlock_voice_aware(void)
{
	int i, j;

#if defined(GPIO_VIBRATE_TRIGGER)	
	nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, false);
#endif
	nrf_drv_gpiote_in_event_enable(GPIO_GSENSOR_INT, false);
	for (i = 0; i < 2; i++)
	{
		for (j = 0; j < 250; j++)
		{
			wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_ON);
			nrf_delay_us(100);
			wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_OFF);
			nrf_delay_us(200);
		}
		nrf_delay_ms(500);
	}
#if defined(GPIO_VIBRATE_TRIGGER)	
	nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, true);
#endif
	kxcjk1013_interrupt_release();
	nrf_drv_gpiote_in_event_enable(GPIO_GSENSOR_INT, true);
}

static void wlock_voice_startup(void)
{
	int i, j;

	for (i = 0; i < 1; i++)
	{
		for (j = 0; j < 250; j++)
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
	int i, j, k, m;

	for (m = 0; m < 56; m++)
	{
		for (k = 10; k < 60; k++)
		{
			for (j = 0; j < 20; j++)
			{
			    if (wlock_ble_connected())
			    {
			        return;
			    }
				wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_ON);
				nrf_delay_us(150);
				wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_OFF);
				for (i = 0; i < k; i++)
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
	switch (cause)
	{
	case WLOCK_GSM_POWER_ON_WARNING:
		break;
	case WLOCK_GSM_POWER_ON_LVD:
		wlock_gpio_set(GPIO_GSM_LOW_POWER_INDICATE, BOOL_GSM_LVD_ON);
		break;
	}
}

static void wlock_led_toggle(void)
{
	static bool led_on = false;

	if (wlock_ble_connected())
	{
		if (led_on == true) {
			led_on = false;
		}
		else
		{
			wlock_gpio_set(WLOCK_BLE_CONNECTED_LED, BOOL_LED_ON);
			nrf_delay_ms(10);
			wlock_gpio_set(WLOCK_BLE_CONNECTED_LED, BOOL_LED_OFF);
			led_on = true;
		}
	}
}

static void wlock_enable_warning_event(bool enable)
{
	if (enable)
	{
		m_wlock_data.aware_flag = false;
		m_wlock_data.aware_interval = 0;
		m_wlock_data.warning_flag = false;
		m_wlock_data.warning_filter = 0;
		m_wlock_data.warning_interval = 0;
		nrf_drv_gpiote_in_event_enable(GPIO_INFRARED_TRIGGER, true);
#if defined(GPIO_VIBRATE_TRIGGER)	
		nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, true);
#endif
		nrf_drv_gpiote_in_event_enable(GPIO_LOCK_PICKING, true);
		kxcjk1013_interrupt_release();
		nrf_drv_gpiote_in_event_enable(GPIO_GSENSOR_INT, true);
	}
	else
	{
		nrf_drv_gpiote_in_event_enable(GPIO_INFRARED_TRIGGER, false);
#if defined(GPIO_VIBRATE_TRIGGER)	
		nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, false);
#endif
		nrf_drv_gpiote_in_event_enable(GPIO_LOCK_PICKING, false);
		nrf_drv_gpiote_in_event_enable(GPIO_GSENSOR_INT, false);
	}
}

static void wlock_test_mode_timer_handler(void)
{
	switch (m_wlock_data.test_item)
	{
	case WLOCK_TEST_MOTION:
		nrf_drv_gpiote_in_event_disable(GPIO_INFRARED_TRIGGER);
#if defined(GPIO_VIBRATE_TRIGGER)	
		nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, true);
#endif
		nrf_drv_gpiote_in_event_disable(GPIO_LOCK_PICKING);
		kxcjk1013_interrupt_release();
		nrf_drv_gpiote_in_event_enable(GPIO_GSENSOR_INT, true);
		break;
	case WLOCK_TEST_INFRARED:
		nrf_drv_gpiote_in_event_enable(GPIO_INFRARED_TRIGGER, true);
#if defined(GPIO_VIBRATE_TRIGGER)	
		nrf_drv_gpiote_in_event_disable(GPIO_VIBRATE_TRIGGER);
#endif
		nrf_drv_gpiote_in_event_disable(GPIO_LOCK_PICKING);
		nrf_drv_gpiote_in_event_disable(GPIO_GSENSOR_INT);
		break;
	case WLOCK_TEST_PICKING:
		nrf_drv_gpiote_in_event_disable(GPIO_INFRARED_TRIGGER);
#if defined(GPIO_VIBRATE_TRIGGER)	
		nrf_drv_gpiote_in_event_disable(GPIO_VIBRATE_TRIGGER);
#endif
		nrf_drv_gpiote_in_event_enable(GPIO_LOCK_PICKING, true);
		nrf_drv_gpiote_in_event_disable(GPIO_GSENSOR_INT);
		break;
	case WLOCK_TEST_MODE_END:
		nrf_drv_gpiote_in_event_disable(GPIO_KEY);
		wlock_enable_warning_event(true);
		m_wlock_data.in_test_mode_flag = false;
		m_wlock_data.test_item = WLOCK_TEST_MODE_START;
		break;
	default:
		break;
	}
}

static void wlock_key_event_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
	nrf_delay_ms(10);
	if (wlock_gpio_get(GPIO_KEY) == BOOL_IS_KEY_PRESS)
	{
		if (m_wlock_data.in_test_mode_flag == false)
		{
			m_wlock_data.in_test_mode_flag = true;
		}
		m_wlock_data.test_item++;
    	m_wlock_data.test_mode_timeout = WLOCK_TEST_MODE_TIMEOUT;
    	m_wlock_data.test_mode_key_event = true;
    	wlock_gpio_set(WLOCK_TEST_MOTION_LED, BOOL_LED_ON);
    	wlock_gpio_set(WLOCK_TEST_INFRARED_LED, BOOL_LED_ON);
    	wlock_gpio_set(WLOCK_TEST_PICKING_LED, BOOL_LED_ON);
    	nrf_delay_ms(100);
    	wlock_gpio_set(WLOCK_TEST_MOTION_LED, BOOL_LED_OFF);
    	wlock_gpio_set(WLOCK_TEST_INFRARED_LED, BOOL_LED_OFF);
    	wlock_gpio_set(WLOCK_TEST_PICKING_LED, BOOL_LED_OFF);
    }
}

static void wlock_gpio_event_handler(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
	if (m_wlock_data.in_test_mode_flag == false)
	{
		switch (pin) {
			case GPIO_GSENSOR_INT:
				if (m_wlock_data.aware_flag == false) 
				{
			    	nrf_delay_ms(2000);
				}
				kxcjk1013_interrupt_release();
			case GPIO_INFRARED_TRIGGER:
#if defined(GPIO_VIBRATE_TRIGGER)	
			case GPIO_VIBRATE_TRIGGER:
#endif
			if (m_wlock_data.aware_flag == false) {
				m_wlock_data.aware_flag = true;
				m_wlock_data.warning_filter++;
			}
			else if (m_wlock_data.warning_filter >= WLOCK_WARNING_FILTER_COUNT)
			{
				m_wlock_data.warning_flag = true;
			}
			else
			{
				m_wlock_data.warning_filter++;
			}
			break;
			case GPIO_LOCK_PICKING:
				m_wlock_data.aware_flag = true;
//				m_wlock_data.warning_flag = true;
			break;
		}	
	}
	else
	{
		switch (pin) {
			case GPIO_GSENSOR_INT:
				kxcjk1013_interrupt_release();
				m_wlock_data.event_gsensor_flag = true;
				break;
			case GPIO_INFRARED_TRIGGER:
				m_wlock_data.event_infrared_flag = true;
				break;
#if defined(GPIO_VIBRATE_TRIGGER)	
			case GPIO_VIBRATE_TRIGGER:
				m_wlock_data.event_vibrate_flag = true;
				break;
#endif
			case GPIO_LOCK_PICKING:
				m_wlock_data.event_lock_picking_flag = true;
				break;
		}

		if (m_wlock_data.event_gsensor_flag || m_wlock_data.event_vibrate_flag)
		{
			wlock_gpio_set(WLOCK_TEST_MOTION_LED, BOOL_LED_ON);
			nrf_delay_ms(100);
			wlock_gpio_set(WLOCK_TEST_MOTION_LED, BOOL_LED_OFF);
			nrf_delay_ms(10);
			m_wlock_data.event_gsensor_flag = false;
			m_wlock_data.event_vibrate_flag = false;
		}
		else if (m_wlock_data.event_infrared_flag)
		{
			wlock_gpio_set(WLOCK_TEST_INFRARED_LED, BOOL_LED_ON);
			nrf_delay_ms(100);
			wlock_gpio_set(WLOCK_TEST_INFRARED_LED, BOOL_LED_OFF);
			nrf_delay_ms(10);
			m_wlock_data.event_infrared_flag = false;
		}
		else if (m_wlock_data.event_lock_picking_flag)
		{
			wlock_gpio_set(WLOCK_TEST_PICKING_LED, BOOL_LED_ON);
			nrf_delay_ms(100);
			wlock_gpio_set(WLOCK_TEST_PICKING_LED, BOOL_LED_OFF);
			nrf_delay_ms(10);
			m_wlock_data.event_lock_picking_flag = false;
		}
	}
}
static void wlock_sec_timer_handler(void * p_context)
{
	static int ble_scanning_timeout = 0;

	if (m_wlock_data.in_test_mode_flag)
	{
		if (m_wlock_data.test_mode_timeout > 0)
		{
			m_wlock_data.test_mode_timeout--;
			if (m_wlock_data.test_mode_key_event == true)
			{
				m_wlock_data.test_mode_key_event = false;
				wlock_test_mode_timer_handler();
			}

		}
		else
		{
			nrf_drv_gpiote_in_event_disable(GPIO_KEY);
			wlock_enable_warning_event(true);
			m_wlock_data.in_test_mode_flag = false;
			m_wlock_data.test_item = WLOCK_TEST_MODE_START;
		}
		return;
	}

	wlock_led_toggle();
	switch (m_wlock_data.wlock_state)
	{
	case WLOCK_STATE_IDLE:
		if (wlock_ble_connected())
		{
			wlock_enable_warning_event(false);
			m_wlock_data.wlock_state = WLOCK_STATE_BLE_CONNECTED;
		}
		else if (m_wlock_data.aware_flag)
		{
			/* enter aware state */
			m_wlock_data.aware_interval = WLOCK_AWARE_INTERVAL;
			m_wlock_data.wlock_state = WLOCK_STATE_AWARE;
			wlock_voice_aware();
		}
		else if((wlock_gpio_get(GPIO_LOCK_PICKING) == BOOL_IS_LOCK_PICKING))
		{
			m_wlock_data.aware_flag = true;
//			m_wlock_data.warning_flag = true;
		}
		else if (wlock_in_charge_state())
		{
			m_wlock_data.lvd_warning_interval = 0;
			m_wlock_data.lvd_rewarning_interval = 0;
		}
		else if (wlock_in_lvd_state())
		{
			if (m_wlock_data.lvd_rewarning_interval == 0)
			{
				wlock_enable_warning_event(false);
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
			m_wlock_data.lvd_warning_interval = 0;
			m_wlock_data.lvd_rewarning_interval = 0;
		}
		break;
	case WLOCK_STATE_AWARE:
		if (wlock_ble_connected())
		{
			wlock_enable_warning_event(false);
			m_wlock_data.wlock_state = WLOCK_STATE_BLE_CONNECTED;
		}
		else if (m_wlock_data.warning_flag)
		{
			/* enter ble scanning state */
			wlock_enable_warning_event(false);
			ble_scanning_timeout = 0;
			scan_start();
			m_wlock_data.wlock_state = WLOCK_STATE_BLE_SCANNING;
		}
		else if((wlock_gpio_get(GPIO_LOCK_PICKING) == BOOL_IS_LOCK_PICKING))
		{
//			m_wlock_data.aware_flag = true;
			m_wlock_data.warning_flag = true;
		}
		else if (m_wlock_data.aware_interval > 0)
		{
			m_wlock_data.aware_interval--;
		}
		else
		{
			wlock_enable_warning_event(true);
			m_wlock_data.wlock_state = WLOCK_STATE_IDLE;
		}
		break;
	case WLOCK_STATE_BLE_SCANNING:
		if (wlock_ble_connected())
		{
			wlock_enable_warning_event(false);
			m_wlock_data.wlock_state = WLOCK_STATE_BLE_CONNECTED;
		}
		else if (ble_scanning_timeout >= WLOCK_BLE_SCAN_TIMEOUT)
		{
			wlock_gsm_power_on(WLOCK_GSM_POWER_ON_WARNING);
			m_wlock_data.warning_interval = WLOCK_WARNING_INTERVAL;
			m_wlock_data.wlock_state = WLOCK_STATE_WARNING;
			wlock_voice_warning();
		}
		else
		{
			ble_scanning_timeout++;
		}
		break;
	case WLOCK_STATE_BLE_CONNECTED:
		if (!wlock_ble_connected())
		{
			wlock_gpio_set(GPIO_LED1, BOOL_LED_OFF);
			scan_start();
			nrf_delay_ms(3000);
			wlock_enable_warning_event(true);
			m_wlock_data.wlock_state = WLOCK_STATE_IDLE;
		}
		else if (wlock_in_lvd_state())
		{
			if (m_wlock_data.lvd_rewarning_interval == 0)
			{
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
	case WLOCK_STATE_WARNING:
#if 0
		if (m_wlock_data.warning_interval > 0)
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
			wlock_enable_warning_event(true);
			m_wlock_data.aware_interval = WLOCK_AWARE_INTERVAL;
			m_wlock_data.wlock_state = WLOCK_STATE_AWARE;
		}
#else
		wlock_gpio_set(GPIO_GSM_POWER_KEY, BOOL_GSM_PWRKEY_OFF);
		wlock_gpio_set(GPIO_GSM_POWER_ON, BOOL_GSM_PWRON_OFF);
		wlock_enable_warning_event(true);
		m_wlock_data.aware_interval = WLOCK_AWARE_INTERVAL;
		m_wlock_data.wlock_state = WLOCK_STATE_AWARE;
#endif
		break;
	case WLOCK_STATE_LVD:
		if (m_wlock_data.lvd_warning_interval > 0)
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
			wlock_enable_warning_event(true);
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
	twi_master_init();
	kxcjk1013_init();
	wlock_endnode_init();
#if 0
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

	/* low power detect */
	config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
	config.pull = NRF_GPIO_PIN_PULLUP;
	err_code = nrf_drv_gpiote_in_init(GPIO_LOW_VOLTAGE_DETECT, &config, wlock_gpio_event_handler);
	if (err_code != NRF_SUCCESS)
	{
		return err_code;
	}
#else
	nrf_gpio_cfg_input(GPIO_CHARGE_STATE, NRF_GPIO_PIN_PULLUP);
	nrf_gpio_cfg_input(GPIO_LOW_VOLTAGE_DETECT, NRF_GPIO_PIN_PULLUP);

#endif
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

#if defined(GPIO_VIBRATE_TRIGGER)	
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
#endif

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

	/* gsensor int */
	config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
	config.pull = NRF_GPIO_PIN_PULLUP;
	err_code = nrf_drv_gpiote_in_init(GPIO_GSENSOR_INT, &config, wlock_gpio_event_handler);
	if (err_code != NRF_SUCCESS)
	{
		return err_code;
	}

	/* key */
	config.is_watcher = false;
	config.hi_accuracy = false;
	config.sense = NRF_GPIOTE_POLARITY_HITOLO;
	config.pull = NRF_GPIO_PIN_PULLUP;
	err_code = nrf_drv_gpiote_in_init(GPIO_KEY, &config, wlock_key_event_handler);
	if (err_code != NRF_SUCCESS)
	{
		return err_code;
	}

	/* LED1 */
	NRF_GPIO->PIN_CNF[GPIO_LED1] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
		| (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)
		| (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
		| (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
		| (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
	wlock_gpio_set(GPIO_LED1, BOOL_LED_ON);
	nrf_delay_ms(10);
	wlock_gpio_set(GPIO_LED1, BOOL_LED_OFF);

	/* LED2 */
	NRF_GPIO->PIN_CNF[GPIO_LED2] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
		| (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)
		| (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
		| (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
		| (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
	wlock_gpio_set(GPIO_LED2, BOOL_LED_ON);
	nrf_delay_ms(10);
	wlock_gpio_set(GPIO_LED2, BOOL_LED_OFF);

	/* LED3 */
	NRF_GPIO->PIN_CNF[GPIO_LED3] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
		| (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)
		| (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
		| (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
		| (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
	wlock_gpio_set(GPIO_LED3, BOOL_LED_ON);
	nrf_delay_ms(10);
	wlock_gpio_set(GPIO_LED3, BOOL_LED_OFF);

	/* voice */
	NRF_GPIO->PIN_CNF[GPIO_SPERAKER] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
		| (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)
		| (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
		| (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
		| (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
	wlock_gpio_set(GPIO_SPERAKER, BOOL_SPEAKER_OFF);


	/* GSM low power indicate */
	NRF_GPIO->PIN_CNF[GPIO_GSM_LOW_POWER_INDICATE] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
		| (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)
		| (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
		| (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
		| (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
	wlock_gpio_set(GPIO_GSM_LOW_POWER_INDICATE, BOOL_GSM_LVD_OFF);


	/* GSM power on */
	NRF_GPIO->PIN_CNF[GPIO_GSM_POWER_ON] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
		| (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)
		| (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
		| (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
		| (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
	wlock_gpio_set(GPIO_GSM_POWER_ON, BOOL_GSM_PWRON_OFF);


	/* GSM power key */
	NRF_GPIO->PIN_CNF[GPIO_GSM_POWER_KEY] = (GPIO_PIN_CNF_SENSE_Disabled << GPIO_PIN_CNF_SENSE_Pos)
		| (GPIO_PIN_CNF_DRIVE_H0H1 << GPIO_PIN_CNF_DRIVE_Pos)
		| (GPIO_PIN_CNF_PULL_Disabled << GPIO_PIN_CNF_PULL_Pos)
		| (GPIO_PIN_CNF_INPUT_Disconnect << GPIO_PIN_CNF_INPUT_Pos)
		| (GPIO_PIN_CNF_DIR_Output << GPIO_PIN_CNF_DIR_Pos);
	wlock_gpio_set(GPIO_GSM_POWER_KEY, BOOL_GSM_PWRKEY_OFF);

	memset(&m_wlock_data, 0, sizeof(wlock_data_t));
	m_wlock_data.aware_flag = false;
	m_wlock_data.wlock_state = WLOCK_STATE_IDLE;
	nrf_drv_gpiote_in_event_enable(GPIO_CHARGE_STATE, false);
	nrf_drv_gpiote_in_event_enable(GPIO_LOW_VOLTAGE_DETECT, false);
	wlock_voice_startup();

	if (wlock_gpio_get(GPIO_KEY) == BOOL_IS_KEY_PRESS)
	{
		nrf_drv_gpiote_in_event_enable(GPIO_KEY, true);
	}
	else
	{
		nrf_delay_ms(2000); /* wait for infrared to be stable */
		nrf_drv_gpiote_in_event_enable(GPIO_INFRARED_TRIGGER, true);
#if defined(GPIO_VIBRATE_TRIGGER)	
		nrf_drv_gpiote_in_event_enable(GPIO_VIBRATE_TRIGGER, true);
#endif
		nrf_drv_gpiote_in_event_enable(GPIO_LOCK_PICKING, true);
		kxcjk1013_interrupt_release();
		nrf_drv_gpiote_in_event_enable(GPIO_GSENSOR_INT, true);
	}

	err_code = app_timer_create(&m_wlock_sec_timer_id, APP_TIMER_MODE_REPEATED, wlock_sec_timer_handler);
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
	if (wlock_endnode_match(node) == true)
	{
		ret = true;
	}
	else if (rssi >= WLOCK_BLE_RSSI)
	{
		ret = wlock_endnode_add(node);
	}
	return ret;
}

#include "ble_rscs.h"
extern ble_rscs_t   m_rscs;
static bool wlock_ble_tx_handler(uint8_t *data, uint16_t len)
{
	uint32_t err_code;
	uint16_t               hvx_len;
	ble_gatts_hvx_params_t hvx_params;

	if (m_rscs.conn_handle != BLE_CONN_HANDLE_INVALID)
	{
		hvx_len = len;
		memset(&hvx_params, 0, sizeof(hvx_params));
		hvx_params.handle = m_rscs.meas_handles.value_handle;
		hvx_params.type = BLE_GATT_HVX_NOTIFICATION;
		hvx_params.offset = 0;
		hvx_params.p_len = &hvx_len;
		hvx_params.p_data = data;

		err_code = sd_ble_gatts_hvx(m_rscs.conn_handle, &hvx_params);
		if ((err_code == NRF_SUCCESS) && (hvx_len != len))
		{
			err_code = NRF_ERROR_DATA_SIZE;
		}
	}
	else
	{
		err_code = NRF_ERROR_INVALID_STATE;
	}

	if (err_code == NRF_SUCCESS)
	{
		return true;
	}
	else
	{
		return false;
	}
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

	uint8_t buf[10];

	if (len == 13)
	{
		if ((data[0] == (PRO_HEAD & 0xff))
			&& (data[1] == ((PRO_HEAD & 0xff00) >> 8))
			&& (data[2] == 0x01))
		{
			memset(buf, 0, sizeof(buf));
			if (wlock_is_allowed_to_connect(&data[4], (int8_t)data[10]) == true)
			{
				buf[0] = (uint8_t)(PRO_HEAD & 0xff);
				buf[1] = (uint8_t)((PRO_HEAD >> 8) & 0xff);
				buf[2] = data[2];
				buf[3] = 1;
				buf[4] = 0; //success
				buf[5] = (uint8_t)(PRO_TAIL & 0xff);
				buf[6] = (uint8_t)((PRO_TAIL >> 8) & 0xff);
				//if(wlock_ble_tx_handler(buf, 7))
				//{
					//m_wlock_data.ble_p_connected_flag = true;
				//}
				wlock_ble_tx_handler(buf, 7);
				m_wlock_data.ble_p_connected_flag = true;
			}
			else
			{
				buf[0] = (uint8_t)(PRO_HEAD & 0xff);
				buf[1] = (uint8_t)((PRO_HEAD >> 8) & 0xff);
				buf[2] = data[2];
				buf[3] = 1;
				buf[4] = 1; //failed, low RSSI
				buf[5] = (uint8_t)(PRO_TAIL & 0xff);
				buf[6] = (uint8_t)((PRO_TAIL >> 8) & 0xff);
				wlock_ble_tx_handler(buf, 7);
			}
		}
	}
}


#endif /* __SUPPORT_WLOCK__ */


