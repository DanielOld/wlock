
#ifndef WLOCK_KEY_H__
#define WLOCK_KEY_H__

#include <stdint.h>
#include <stdbool.h>

#define GPIO_KEY		 				17
#define GPIO_LED1						18  
#define BOOL_LED_ON						0
#define BOOL_LED_OFF					1

uint32_t wlock_key_init(void);


#endif /* WLOCK_KEY_H__ */

