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

#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

#include "../kernel/vport.h"

#define MAX(a, b)   \
    ({ int ___a = (a); int ___b = (b); ___a > ___b ? ___a : ___b; })

static int sock_fd = 0;

static int vport_init(void)
{
    struct sockaddr_nl src_addr = { 0 };

    if ((sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_VPORT)) < 0) {
        perror("Failed creating a netlink socket (module not in the kernel?)");
        return -1;
    }

    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();

    if (bind(sock_fd, (struct sockaddr *) &src_addr, sizeof(src_addr)) < 0) {
        perror("Failed binding");
        close(sock_fd);
        return -1;
    }

    return 0;
}

static int communicate(struct nlmsghdr *nlh)
{
    struct iovec iov = { 0 };
    struct msghdr msg = { 0 };
    struct sockaddr_nl dest_addr = { 0 };

    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0; /* For Linux Kernel */
    dest_addr.nl_groups = 0; /* unicast */
    iov.iov_base = (void *) nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *) &dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (sendmsg(sock_fd, &msg, 0) < 0) {
        perror("Failed sending message to the kernel");
        return -1;
    }
    if (recvmsg(sock_fd, &msg, 0) < 0) {
        perror("Failed receiving reply from the kernel");
        return -1;
    }

    return 0;
}

static struct nlmsghdr *alloc_nlmsghdr(int msglen)
{
    struct nlmsghdr *nlh = NULL;

    if ((nlh = (struct nlmsghdr *) calloc(1, NLMSG_SPACE(msglen)))) {
        nlh->nlmsg_len = NLMSG_SPACE(msglen);
        nlh->nlmsg_pid = getpid();
        nlh->nlmsg_flags = 0;
    }

    return nlh;
}

static int verify_port_name_len(char *port)
{
    if (strlen(port) >= IFNAMSIZ) {
        printf("Port name %s is too long (should be no more than %d "
                "characters)\n", port, IFNAMSIZ - 1);
        return -1;
    }
    return 0;
}

#define ERR_E2S(e)      \
        [e] = #e
static char *err2str[VPORT_ERR_MAX] = {
    ERR_E2S(VPORT_ERR_OK),
    ERR_E2S(VPORT_ERR_PORT_ALREADY_EXISTS),
    ERR_E2S(VPORT_ERR_UNKNOWN_ACTION),
    ERR_E2S(VPORT_ERR_CANNOT_REGISTER_DEVICE),
    ERR_E2S(VPORT_ERR_NO_SUCH_DEVICE),
    ERR_E2S(VPORT_ERR_DEVICE_BUSY),
    ERR_E2S(VPORT_ERR_NO_MEM),
};

static void handle_ports(char *port1, char *port2, 
        vport_action_type_t action)
{
    vport_request_t *req = NULL;
    vport_reply_t *rep = NULL;
    struct nlmsghdr *nlh = NULL;
    int msglen = MAX(sizeof(*req), sizeof(*rep));

    if (!port1 || verify_port_name_len(port1))
        return;

    nlh = alloc_nlmsghdr(msglen);
    req = NLMSG_DATA(nlh);
    req->action = action;
    strcpy(req->ports[0], port1);
    if (port2)
        strcpy(req->ports[1], port2);

    if (!communicate(nlh)) {
        rep = (vport_reply_t *)NLMSG_DATA(nlh);
        printf("%s(%d) %s\n", err2str[rep->err], rep->err,
                        rep->port[0] ? rep->port : "");
    } else {
        printf("Something went wrong when communicating with the kernel\n");
    }

    free(nlh);
    return;
}

static void connect_ports(char *ports)
{
    char *_ports = strdup(ports);
    char *comma = strchr(_ports, ',');
    char *port1 = NULL, *port2 = NULL;

    if (!comma) {
        printf("Ports need to be comma-separated '%s'\n", ports);
        goto exit;
    }

    *comma = 0;
    port1 = _ports;
    port2 = comma;
    port2++;
    if (verify_port_name_len(port2))
        goto exit;
    handle_ports(port1, port2, VPORT_ACTION_TYPE_CONNECT);
exit:
    free(_ports);
    return;
}

static void dump_ports(char *name)
{
        handle_ports(name, NULL, VPORT_ACTION_TYPE_DUMP);
}

static void usage(void)
{
    printf(
        "Options:\n"
         "\t-a|--add-port <Port name>:\n"
             "\t\tAdds a port with <Port name>\n"
         "\t-r|--remove-port <Port name>:\n"
             "\t\tRemove a port with <Port name>. The port must be "
             "disconnected.\n"
         "\t-c|--connect-ports <Port name>,<Port name>:\n"
             "\t\tConnect 2 ports\n"
         "\t-d|--disconnect-port <Port name>:\n"
             "\t\tDisconnect a port (will also automatically disconnect the \n"
             "\t\tport that was connected to the port\n"
         "\t-D|--dump <Port name>:\n"
             "\t\tDump info about a port\n"
         "\t-h|--help:\n"
            "\t\tDisplay this help and exit\n\n"
         "\tThere is no mutual exclusion of any kind between the options, the \n"
         "\tuser can add a port and remove a port in the same invocation "
         "etc.\n\n");
    exit(0);
}

/*
 */
int main(int argc, char *argv[])
{
    int ret = 0;
    int c;

    if ((ret = vport_init()))
        return ret;

    while (1)
    {
        static struct option long_options[] =
        {
            {"add-port", required_argument, 0, 'a'},
            {"remove-port", required_argument, 0, 'r'},
            {"connect-ports", required_argument, 0, 'c'},
            {"disconnect-port", required_argument, 0, 'd'},
            {"dump", required_argument, 0, 'D'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        int option_index = 0;

        c = getopt_long(argc, argv, "a:r:c:d:D:h", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c)
        {
        default:
            printf("Unknown option %c\n", c);
            break;
        case 'a':
        case 'r':
        case 'd':
            {
                vport_action_type_t action = c == 'a' ? 
                    VPORT_ACTION_TYPE_ADD : (c == 'r' ? 
                            VPORT_ACTION_TYPE_REMOVE : 
                            VPORT_ACTION_TYPE_DISCONNECT);
                handle_ports(optarg, NULL, action);
                break;
            }
        case 'c':
            connect_ports(optarg);
            break;
        case 'D':
            dump_ports(optarg);
            break;
        case 'h':
            usage();
            break;
        }
    }
    close(sock_fd);

    return 0;
}
