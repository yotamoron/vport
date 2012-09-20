/*
 * This file is part of vport.
 * 
 * vport is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * vport is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with vport.  If not, see <http://www.gnu.org/licenses/>.
 */

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
