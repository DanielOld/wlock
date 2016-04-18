
#ifndef WLOCK_KEY_H__
#define WLOCK_KEY_H__

#include <stdint.h>
#include <stdbool.h>

#define GPIO_KEY		 				0
#define GPIO_LED1						19  
#define BOOL_LED_ON						1
#define BOOL_LED_OFF					0

uint32_t wlock_key_init(void);


#endif /* WLOCK_KEY_H__ */

