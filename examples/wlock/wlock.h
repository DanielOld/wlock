
#ifndef WLOCK_H__
#define WLOCK_H__

#include <stdint.h>
#include <stdbool.h>

#include <ble_gap.h>

typedef struct
{
  uint8_t addr[BLE_GAP_ADDR_LEN];       /**< 48-bit address, LSB format. */
} wlock_endnode_t;


uint32_t wlock_init(void);


#endif /* WLOCK_H__ */

