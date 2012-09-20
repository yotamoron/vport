
#include "../common/vport.h"

#include <linux/module.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>

struct sock *nl_sk = NULL;

static void vport_nl_recv_msg(struct sk_buff *skb) {

    struct nlmsghdr *nlh;
    int pid;
    struct sk_buff *skb_out;
    int msg_size;
    vport_request_t *req = NULL;
    vport_reply_t *rep = NULL;
    vport_err_t rc = VPORT_ERR_OK;

    msg_size = sizeof(vport_reply_t);

    nlh = (struct nlmsghdr *) skb->data;
    pid = nlh->nlmsg_pid; /*pid of sending process */

    req = (vport_request_t *) nlmsg_data(nlh);
    switch (req->action) {
    case VPORT_ACTION_TYPE_ADD:
        printk("Adding port %s\n", req->ports[0]);
        rc = VPORT_ERR_OK;
        break;
    case VPORT_ACTION_TYPE_REMOVE:
        printk("Removing port %s\n", req->ports[0]);
        rc = VPORT_ERR_OK;
        break;
    case VPORT_ACTION_TYPE_CONNECT:
        printk("Connecting ports %s<-->%s\n", req->ports[0], req->ports[1]);
        rc = VPORT_ERR_OK;
        break;
    case VPORT_ACTION_TYPE_DISCONNECT:
        printk("Disconnecting port %s\n", req->ports[0]);
        rc = VPORT_ERR_OK;
        break;
    case VPORT_ACTION_TYPE_DUMP:
        printk("Dumping\n");
        rc = VPORT_ERR_OK;
        break;
    default:
        rc = VPORT_ERR_UNKNOWN_ACTION;
        break;
    }
    skb_out = nlmsg_new(msg_size, 0);
    if(!skb_out) {
        printk(KERN_ERR "Failed to allocate new skb\n");
        return;
    } 
    nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, msg_size, 0);
    NETLINK_CB(skb_out).dst_group = 0;
    rep = (vport_reply_t *) nlmsg_data(nlh);
    rep->err = rc;

    if(nlmsg_unicast(nl_sk, skb_out, pid) < 0)
        printk(KERN_INFO "Error while sending bak to user\n");
}

static int __init kvport_init(void) {

    printk("Entering: %s\n",__FUNCTION__);
    nl_sk = netlink_kernel_create(&init_net, NETLINK_VPORT, 0, vport_nl_recv_msg,
            NULL, THIS_MODULE);
    if(!nl_sk)
    {
        printk(KERN_ALERT "Error creating socket.\n");
        return -10;
    }

    return 0;
}

static void __exit kvport_exit(void) 
{
    printk(KERN_INFO "exiting kvport module\n");
    netlink_kernel_release(nl_sk);
}

module_init(kvport_init);
module_exit(kvport_exit);

MODULE_LICENSE("GPL");

