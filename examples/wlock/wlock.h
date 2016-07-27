
#ifndef WLOCK_H__
#define WLOCK_H__

#include <stdint.h>
#include <stdbool.h>

#define WLOCK_MAX_ENDNODE				10

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
	WLOCK_STATE_WARNING,
	WLOCK_STATE_LVD
} wlock_state_t;

typedef enum
{
    WLOCK_GSM_POWER_ON_WARNING,
	WLOCK_GSM_POWER_ON_LVD
} wlock_power_on_cause_t;


#define WLOCK_BLE_SCAN_TIMEOUT          3 /* sec */  
#define WLOCK_BLE_RSSI				  (-70) /* dBm */
#if 0
/* input GPIO */
#define GPIO_CHARGE_STATE 				4
#define GPIO_LOW_VOLTAGE_DETECT 		13
#define GPIO_INFRARED_TRIGGER			16
#define GPIO_VIBRATE_TRIGGER			15
#define GPIO_LOCK_PICKING				14 
#define GPIO_GSENSOR_INT				24
#define GPIO_ERASE_KEY					17

/* output GPIO */
#define GPIO_LED1						18  
#define GPIO_SPERAKER					7
#define GPIO_INFRARED_POWER_ON			11 /* Control infrared power */
#define GPIO_GSM_LOW_POWER_INDICATE		3  /* Indicate low power state */
#define GPIO_GSM_POWER_ON				10 /* Open V_BAT */
#define GPIO_GSM_POWER_KEY				6 /* GSM power key */

/* uart GPIO */
#define RX_PIN_NUMBER  1
#define TX_PIN_NUMBER  2

#else /* for development board pca10028 */
#define GPIO_CHARGE_STATE 				17
#define GPIO_LOW_VOLTAGE_DETECT 		18
#define GPIO_INFRARED_TRIGGER			19
#define GPIO_VIBRATE_TRIGGER			20
#define GPIO_LOCK_PICKING				21 
#define GPIO_GSENSOR_INT				22
#define GPIO_ERASE_KEY					23

/* output GPIO */
#define GPIO_LED1						3  
#define GPIO_SPERAKER					4
#define GPIO_INFRARED_POWER_ON			5 /* Control infrared power */
#define GPIO_GSM_LOW_POWER_INDICATE		6  /* Indicate low power state */
#define GPIO_GSM_POWER_ON				7 /* Open V_BAT */
#define GPIO_GSM_POWER_KEY				8 /* GSM power key */

/* uart GPIO */
#define RX_PIN_NUMBER  1
#define TX_PIN_NUMBER  2
#endif
/* input */
#define BOOL_IS_CHR						1
#define BOOL_IS_LVD						0
#define BOOL_IS_ERASE					0

/* output */
#define BOOL_LED_ON						1
#define BOOL_LED_OFF					0
#define BOOL_SPEAKER_ON					1
#define BOOL_SPEAKER_OFF 				0
#define BOOL_INFRARED_POWER_ON 			1
#define BOOL_INFRARED_POWER_OFF			0
#define BOOL_GSM_LVD_ON					1
#define BOOL_GSM_LVD_OFF				0
#define BOOL_GSM_PWRON_ON				1
#define BOOL_GSM_PWRON_OFF				0
#define BOOL_GSM_PWRKEY_ON				1
#define BOOL_GSM_PWRKEY_OFF				0


#define WLOCK_WARNING_FILTER_COUNT 	1
#define WLOCK_AWARE_INTERVAL 	60
#define WLOCK_WARNING_INTERVAL  60
#define WLOCK_GSM_POWER_KEY_INTERVAL 3
#define WLOCK_LVD_WARNING_INTERVAL 60
#define WLOCK_LVD_REWARNING_INTERVAL 7200 /* two hours */

typedef struct
{
	//app_timer_id_t sec_timer_id;
	wlock_state_t wlock_state;

	/* lvd */
    bool lvd_flag; /* low voltage detect flag */
	uint32_t lvd_warning_interval; /* low voltage detect warning interval */
	uint32_t lvd_rewarning_interval; /* low voltage detect rewarning interval */
	
    /* gsm */
	int32_t gsm_power_key_interval;

    /* aware */
	bool aware_flag; /* get infrared or vibrate event for the first time */
	int32_t aware_interval;

	/* warning */
    bool warning_flag; /* infrared and vibrate warning */
	int32_t warning_filter;   /* filter count for infrared and vibrate*/
	int32_t warning_interval;

    bool warning_event_enabled;
	
	bool ble_c_connected_flag; /*central*/
	bool ble_p_connected_flag; /*peripheral*/
} wlock_data_t;

uint32_t wlock_init(void);
bool wlock_is_allowed_to_connect(uint8_t const * addr, int8_t rssi);
bool wlock_endnode_clear(void);
void wlock_ble_rx_handler(uint8_t *data, uint16_t len);


#endif /* WLOCK_H__ */

