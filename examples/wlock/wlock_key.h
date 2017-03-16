
#ifndef WLOCK_KEY_H__
#define WLOCK_KEY_H__

#include <stdint.h>
#include <stdbool.h>

#define GPIO_KEY		 				11
#define GPIO_LED1						12  
#define GPIO_LED2						13  
#define BOOL_LED_ON						1
#define BOOL_LED_OFF					0

uint32_t wlock_key_init(void);
void wlock_key_led_proecess(bool connected);


#endif /* WLOCK_KEY_H__ */

