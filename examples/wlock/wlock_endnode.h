
#ifndef WLOCK_ENDNODE_H__
#define WLOCK_ENDNODE_H__

#include <stdint.h>
#include <stdbool.h>

#define WLOCK_MAX_ENDNODE				50

typedef struct
{
  uint8_t addr[6];       /**< 48-bit address, LSB format. */
} wlock_endnode_t;


bool wlock_endnode_init(void);
bool wlock_endnode_add(wlock_endnode_t endnode);
bool wlock_endnode_match(wlock_endnode_t endnode);


#endif /* WLOCK_ENDNODE_H__ */

