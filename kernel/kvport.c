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

#include "vport.h"

#include <linux/module.h>
#include <linux/netdevice.h>
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_arp.h>

struct vport_priv {
        struct list_head ports;
        struct net_device *this;
        struct net_device *other;
};

#define to_vport_priv(dev) ((struct vport_priv *)netdev_priv(dev))
struct sock *nl_sk = NULL;

LIST_HEAD(vport_list);

static struct net_device *vport_get_by_name(char *name)
{
        struct vport_priv *vp;

        list_for_each_entry(vp, &vport_list, ports) {
                if (!strcmp(vp->this->name, name))
                        return vp->this;
        }
        return NULL;
}

static u32 always_on(struct net_device *dev)
{
	return 1;
}

static const struct ethtool_ops vport_ethtool_ops = {
	.get_link = always_on,
};

static netdev_tx_t vport_xmit(struct sk_buff *skb, struct net_device *dev)
{
        struct vport_priv *vp = to_vport_priv(dev);

	skb_orphan(skb);

        if (vp->other) {
                skb->protocol = eth_type_trans(skb, vp->other);
                netif_rx(skb);
        }

	return NETDEV_TX_OK;
}

static const struct net_device_ops vport_ops = {
	.ndo_start_xmit         = vport_xmit,
};

static void vport_dev_free(struct net_device *dev)
{
        struct vport_priv *vp = to_vport_priv(dev);

        list_del(&vp->ports);        
        free_netdev(dev);
}

static void vport_setup(struct net_device *dev)
{
	dev->mtu		= (16 * 1024) + 20 + 20 + 12;
	dev->hard_header_len	= ETH_HLEN;
	dev->addr_len		= ETH_ALEN;
	dev->tx_queue_len	= 0;
	dev->type		= ARPHRD_ETHER;
	dev->priv_flags	       &= ~IFF_XMIT_DST_RELEASE;
	dev->hw_features	= NETIF_F_ALL_TSO | NETIF_F_UFO;
	dev->features 		= NETIF_F_SG | NETIF_F_FRAGLIST
		| NETIF_F_ALL_TSO
		| NETIF_F_UFO
		| NETIF_F_HW_CSUM
		| NETIF_F_RXCSUM
		| NETIF_F_HIGHDMA
		| NETIF_F_LLTX
		| NETIF_F_NETNS_LOCAL
		| NETIF_F_VLAN_CHALLENGED;
	dev->ethtool_ops	= &vport_ethtool_ops;
	dev->header_ops		= &eth_header_ops;
	dev->netdev_ops		= &vport_ops;
	dev->destructor		= vport_dev_free;
}

static vport_err_t vport_add(char *vport_name)
{
        struct net_device *dev;
        struct vport_priv *vp;

        if (vport_get_by_name(vport_name))
                return VPORT_ERR_PORT_ALREADY_EXISTS;

        dev = alloc_netdev(sizeof(struct vport_priv), vport_name, vport_setup);
        if (register_netdev(dev)) {
                free_netdev(dev);
                return VPORT_ERR_CANNOT_REGISTER_DEVICE;
        }

        vp = to_vport_priv(dev);
        INIT_LIST_HEAD(&vp->ports);
        list_add(&vport_list, &vp->ports);
        vp->this = dev;
        vp->other = NULL;

        return VPORT_ERR_OK;
}

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
                rc = vport_add(req->ports[0]);
                break;
        case VPORT_ACTION_TYPE_REMOVE:
                printk("Removing port %s\n", req->ports[0]);
                rc = VPORT_ERR_OK;
                break;
        case VPORT_ACTION_TYPE_CONNECT:
                printk("Connecting ports %s<-->%s\n", req->ports[0], 
                        req->ports[1]);
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
                printk(KERN_INFO "Error while sending reply to user\n");

        return;
}

static int __init kvport_init(void)
{
        printk("Entering: %s\n",__FUNCTION__);
        nl_sk = netlink_kernel_create(&init_net, NETLINK_VPORT, 0, 
                        vport_nl_recv_msg, NULL, THIS_MODULE);
        if(!nl_sk) {
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

