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
        struct net_device *other;
};

#define to_vport_priv(dev) ((struct vport_priv *)netdev_priv(dev))
struct sock *nl_sk = NULL;

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

static int vport_set_mac(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;

	if (!is_valid_ether_addr(addr->sa_data))
		return -EADDRNOTAVAIL;

	if (netif_running(dev))
		return -EBUSY;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
	dev->addr_assign_type &= ~NET_ADDR_RANDOM;

        return 0;
}

static const struct net_device_ops vport_ops = {
	.ndo_start_xmit         = vport_xmit,
        .ndo_set_mac_address    = vport_set_mac,
};

static void vport_setup(struct net_device *dev)
{
        ether_setup(dev);
	dev->mtu		= (16 * 1024) + 20 + 20 + 12;
	dev->tx_queue_len	= 0;
	dev->priv_flags	       &= ~IFF_XMIT_DST_RELEASE;
	dev->hw_features	= NETIF_F_ALL_TSO | NETIF_F_UFO;
        random_ether_addr(dev->perm_addr);
        memcpy(dev->dev_addr, dev->perm_addr, sizeof(dev->dev_addr));
	dev->addr_assign_type |= NET_ADDR_RANDOM;
	dev->ethtool_ops	= &vport_ethtool_ops;
	dev->netdev_ops		= &vport_ops;
}

static vport_err_t vport_add(char *vport_name)
{
        struct net_device *dev;
        struct vport_priv *vp;

        dev = alloc_netdev(sizeof(struct vport_priv), vport_name, vport_setup);
        if (!dev)
                return VPORT_ERR_NO_MEM;
        if (register_netdev(dev)) {
                free_netdev(dev);
                return VPORT_ERR_CANNOT_REGISTER_DEVICE;
        }

        vp = to_vport_priv(dev);
        vp->other = NULL;

        return VPORT_ERR_OK;
}

static vport_err_t vport_remove(char *name)
{
        struct vport_priv *vp;
        struct net_device *dev;

        dev = dev_get_by_name(&init_net, name);
        if (!dev)
                return VPORT_ERR_NO_SUCH_DEVICE;
        vp = to_vport_priv(dev);
        if (vp->other) {
                dev_put(dev);
                return VPORT_ERR_DEVICE_BUSY;
        }

        dev_put(dev);
	rtnl_lock();
        unregister_netdevice(dev);
	rtnl_unlock();
        free_netdev(dev);
        return VPORT_ERR_OK;
}

static vport_err_t vport_connect(char *p1, char *p2)
{
        struct net_device *dev1, *dev2;
        struct vport_priv *vp1, *vp2;

        dev1 = dev_get_by_name(&init_net, p1);
        if (!dev1)
                return VPORT_ERR_NO_SUCH_DEVICE;
        dev2 = dev_get_by_name(&init_net, p2);
        if (!dev2) {
                dev_put(dev1);
                return VPORT_ERR_NO_SUCH_DEVICE;
        }
        vp1 = to_vport_priv(dev1);
        vp2 = to_vport_priv(dev2);
        if (vp1->other || vp2->other) {
                dev_put(dev1);
                dev_put(dev2);
                return VPORT_ERR_DEVICE_BUSY;
        }
        netif_carrier_on(dev1);
        netif_carrier_on(dev2);
        vp1->other = dev2;
        vp2->other = dev1;

        return VPORT_ERR_OK;
}

static vport_err_t dump_port(char *p1, char *to)
{
        struct net_device *dev;
        struct vport_priv *vp;

        dev = dev_get_by_name(&init_net, p1);
        if (!dev)
                return VPORT_ERR_OK;
        vp = to_vport_priv(dev);
        if (vp->other) {
                memcpy(to, vp->other->name, sizeof(vp->other->name));
        }
        dev_put(dev);
        return VPORT_ERR_OK;
}

static vport_err_t vport_disconnect(char *name)
{
        struct vport_priv *vp1, *vp2;
        struct net_device *dev1, *dev2;

        dev1 = dev_get_by_name(&init_net, name);
        if (!dev1)
                return VPORT_ERR_NO_SUCH_DEVICE;
        vp1 = to_vport_priv(dev1);
        dev_put(dev1);
        if (!vp1->other) {
                dev_put(dev1);
                return VPORT_ERR_OK;
        }
        dev2 = vp1->other;
        vp2 = to_vport_priv(dev2);
        vp1->other = NULL;
        netif_carrier_off(dev2);
        dev_put(dev2);
        vp2->other = NULL;
        netif_carrier_off(dev1);
        dev_put(dev1);

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
        char port[IFNAMSIZ] = { 0 };

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
                rc = vport_remove(req->ports[0]);
                break;
        case VPORT_ACTION_TYPE_CONNECT:
                printk("Connecting ports %s<-->%s\n", req->ports[0], 
                        req->ports[1]);
                rc = vport_connect(req->ports[0], req->ports[1]);
                break;
        case VPORT_ACTION_TYPE_DISCONNECT:
                printk("Disconnecting port %s\n", req->ports[0]);
                rc = vport_disconnect(req->ports[0]);
                break;
        case VPORT_ACTION_TYPE_DUMP:
                printk("Dumping\n");
                rc = dump_port(req->ports[0], port);
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
        memcpy(rep->port, port, sizeof(port));

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

