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
#include "fstorage.h"
//#include "fds.h"

#ifdef __SUPPORT_WLOCK__
#include "wlock_endnode.h"

static void wlock_endnode_evt_handler(uint8_t           op_code,
                        uint32_t          result,
                        uint32_t  const * p_data,
                        fs_length_t       length);

FS_SECTION_VARS_ADD(fs_config_t fs_config) = { .cb = wlock_endnode_evt_handler, .num_pages = 1 };

static wlock_endnode_t g_endnode_mapping[WLOCK_MAX_ENDNODE];

#define ENDNODE_MAPPING_SIZE (sizeof(wlock_endnode_t)*WLOCK_MAX_ENDNODE)
#define WLOCK_ENDNODE_DEFAULT_CHAR 0xff
bool m_endnode_completed = false;


static void wlock_endnode_evt_handler(uint8_t           op_code,
                        uint32_t          result,
                        uint32_t  const * p_data,
                        fs_length_t       length)
{
}

static bool wlock_endnode_load(void)
{
	memcpy(g_endnode_mapping, (wlock_endnode_t*)fs_config.p_start_addr, ENDNODE_MAPPING_SIZE);
	return true;
}

static bool wlock_endnode_clear(void)
{
    fs_erase(&fs_config,
            fs_config.p_start_addr,
            ENDNODE_MAPPING_SIZE/4);
	return true;
}

static bool wlock_endnode_store(void)
{
   wlock_endnode_clear();
   fs_store(&fs_config,
            fs_config.p_start_addr,
            (uint32_t const *)g_endnode_mapping,
            ENDNODE_MAPPING_SIZE/4);
	return true;
}

bool wlock_endnode_match(wlock_endnode_t endnode)
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

bool wlock_endnode_add(wlock_endnode_t endnode)
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
    ret_code_t   fs_ret;
	
    fs_ret = fs_init();
    if (fs_ret != NRF_SUCCESS)
    {
        return false;
    }
    wlock_endnode_load();
	return true;
}

#endif /* __SUPPORT_WLOCK__ */


