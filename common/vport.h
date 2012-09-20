
#ifndef __CONSTANTS_H__
#define __CONSTANTS_H__

#include <linux/if.h>

#define NETLINK_VPORT 31

typedef enum {
    VPORT_ACTION_TYPE_ADD = 0,
    VPORT_ACTION_TYPE_REMOVE = 1,
    VPORT_ACTION_TYPE_CONNECT = 2,
    VPORT_ACTION_TYPE_DISCONNECT = 3,
    VPORT_ACTION_TYPE_DUMP = 4,
    VPORT_ACTION_TYPE_MAX
} vport_action_type_t;

typedef struct {
    vport_action_type_t action;
    char ports[2][IFNAMSIZ];
} vport_message_t;

/* Maximum payload size*/
#define MAX_PAYLOAD 1024

#endif /* __CONSTANTS_H__ */
