
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
} vport_request_t;

typedef enum {
    VPORT_ERR_OK = 0,
    VPORT_ERR_PORT_ALREADY_EXISTS = 1,
    VPORT_ERR_UNKNOWN_ACTION = 2,
    VPORT_ERR_MAX
} vport_err_t;

typedef struct {
    vport_err_t err;
} vport_reply_t;

#endif /* __CONSTANTS_H__ */
