
#ifndef WLOCK_H__
#define WLOCK_H__

#include <stdint.h>
#include <stdbool.h>
#include <ble_gap.h>

#define WLOCK_MAX_ENDNODE				50

typedef struct
{
  uint8_t addr[6];       /**< 48-bit address, LSB format. */
} wlock_endnode_t;

typedef enum
{
    WLOCK_STATE_IDLE,
	WLOCK_STATE_AWARE,
	WLOCK_STATE_BLE_SCANNING,
	WLOCK_STATE_BLE_CONNECTED,
	WLOCK_STATE_BLE_DISCONNECTED,
	WLOCK_STATE_WARNING,
	WLOCK_STATE_LVD
} wlock_state_t;

typedef enum
{
    WLOCK_GSM_POWER_ON_WARNING,
	WLOCK_GSM_POWER_ON_LVD
} wlock_power_on_cause_t;


/* input GPIO */
#define GPIO_CHARGE_STATE 				8
#define GPIO_INFRARED_TRIGGER			9
#define GPIO_VIBRATE_TRIGGER			10
#define GPIO_GSENSOR_TRIGGER			11
#define GPIO_LOW_VOLTAGE_DETECT 		12

/* output GPIO */
#define GPIO_LED1						14
#define GPIO_SPERAKER					15
#define GPIO_INFRARED_POWER_ON			16 /* Control infrared power */
#define GPIO_GSM_LOW_POWER_INDICATE		17 /* Indicate low power state */
#define GPIO_GSM_POWER_ON				18 /* Open V_BAT */
#define GPIO_GSM_POWER_KEY				19 /* GSM power key */

#define BOOL_CHR_ON			1
#define BOOL_LVD_ON			1
#define BOOL_LVD_OFF		0
#define BOOL_PWRON_ON		1
#define BOOL_PWRON_OFF		0
#define BOOL_PWRKEY_ON		1
#define BOOL_PWRKEY_OFF		0

#define WLOCK_WARNING_FILTER_COUNT 	1
#define WLOCK_AWARE_INTERVAL 	60
#define WLOCK_WARNING_INTERVAL  60
#define WLOCK_GSM_POWER_KEY_INTERVAL 3
#define WLOCK_LVD_WARNING_INTERVAL 60
#define WLOCK_LVD_REWARNING_INTERVAL 7200 /* two hours */

typedef struct
{
    wlock_endnode_t endnode_mapping[WLOCK_MAX_ENDNODE];
	app_timer_id_t sec_timer_id;
	wlock_state_t wlock_state;

	/* lvd */
    bool lvd_flag; /* low voltage detect flag */
	uint32_t lvd_warning_interval; /* low voltage detect warning interval */
	uint32_t lvd_rewarning_interval; /* low voltage detect rewarning interval */
	
    /* gsm */
	int32_t gsm_power_key_interval;

	/* for charge */
    bool in_charge_flag; /* in charge flag */

    /* aware */
	bool aware_flag; /* get infrared or vibrate event for the first time */
	int32_t aware_interval;

	/* ble */
	bool ble_connected_flag;
	bool ble_disconnected_flag;
	bool ble_scan_timeout_flag;

	/* warning */
    bool warning_flag; /* infrared and vibrate warning */
	int32_t warning_filter;   /* filter count for infrared and vibrate*/
	int32_t warning_interval;
} wlock_data_t;

uint32_t wlock_init(void);


#endif /* WLOCK_H__ */

