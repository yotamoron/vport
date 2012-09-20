
#include <sys/socket.h>
#include <linux/netlink.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>

#include "../common/vport.h"

struct sockaddr_nl dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
struct msghdr msg;

static int vport_init(int *sock_fd)
{
    struct sockaddr_nl src_addr = { 0 };

    if ((*sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_VPORT)) < 0) {
        perror("Failed creating a netlink socket (module not in the kernel?)");
        return -1;
    }

    src_addr.nl_family = AF_NETLINK;
    src_addr.nl_pid = getpid();

    if (bind(*sock_fd, (struct sockaddr *) &src_addr, sizeof(src_addr)) < 0) {
        perror("Failed binding");
        close(*sock_fd);
        return -1;
    }

    return 0;
}

static void add_port(char *port)
{
    return;
}

static void remove_port(char *port)
{
    return;
}

static void connect_ports(char *ports)
{
    return;
}

static void disconnect_ports(char *ports)
{
    return;
}

static void dump_ports(void)
{
    return;
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
         "\t-D|--dump:\n"
             "\t\tDump info about ports\n"
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
    int sock_fd = 0;
    int c;

    if ((ret = vport_init(&sock_fd)))
        return ret;

    while (1)
    {
        static struct option long_options[] =
        {
            {"add-port", required_argument, 0, 'a'},
            {"remove-port", required_argument, 0, 'r'},
            {"connect-ports", required_argument, 0, 'c'},
            {"disconnect-port", required_argument, 0, 'd'},
            {"dump", no_argument, 0, 'D'},
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
        };
        int option_index = 0;

        c = getopt_long(argc, argv, "a:r:c:d:Dh", long_options, &option_index);

        /* Detect the end of the options. */
        if (c == -1)
            break;

        switch (c)
        {
        default:
            printf("Unknown option %c\n", c);
            break;
        case 'a':
            add_port(optarg);
            break;
        case 'r':
            remove_port(optarg);
            break;
        case 'c':
            connect_ports(optarg);
            break;
        case 'd':
            disconnect_ports(optarg);
            break;
        case 'D':
            dump_ports();
            break;
        case 'h':
            usage();
            break;
        }
    }
    memset(&dest_addr, 0, sizeof (dest_addr));
    dest_addr.nl_family = AF_NETLINK;
    dest_addr.nl_pid = 0; /* For Linux Kernel */
    dest_addr.nl_groups = 0; /* unicast */

    nlh = (struct nlmsghdr *) malloc(NLMSG_SPACE(MAX_PAYLOAD));
    memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
    nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
    nlh->nlmsg_pid = getpid();
    nlh->nlmsg_flags = 0;

    strcpy(NLMSG_DATA(nlh), "Hello");

    iov.iov_base = (void *) nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *) &dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    printf("Sending message to kernel\n");
    sendmsg(sock_fd, &msg, 0);
    printf("Waiting for message from kernel\n");

    /* Read message from kernel */
    recvmsg(sock_fd, &msg, 0);
    printf("Received message payload: %s\n", (char *)NLMSG_DATA(nlh));
    close(sock_fd);

    return 0;
}
