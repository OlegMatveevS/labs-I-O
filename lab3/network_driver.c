#include <linux/module.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/moduleparam.h>
#include <linux/in.h>
#include <net/arp.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <linux/proc_fs.h>
#define BUFFSIZE 1000

static char* link = "lo";
module_param(link, charp, 0);

static char* ifname = "vni%d";

static struct net_device_stats stats;
static struct proc_dir_entry* entry;

static struct net_device *child = NULL;
struct priv {
    struct net_device *parent;
};

int processed_packets = 0;
int dropped_packets = 0;
char *buffer;
size_t buffer_ptr = 0;

void init_buffer(void) 
{
    buffer = (char*) kmalloc(BUFFSIZE * sizeof(char), GFP_KERNEL);
    if (buffer)
    {
        buffer_ptr = 0;
        printk(KERN_INFO "Driver: Successfully allocated result buffer of size %d\n", BUFFSIZE);
        return;
    }
    printk(KERN_ERR "Driver: Failed to allocate result buffer %d\n", BUFFSIZE);
}

static ssize_t proc_read(struct file *file, char __user * ubuf, size_t count, loff_t* ppos)
{
    size_t len = strlen(buffer);
    if (*ppos > 0 || count < len) return 0;
    if (copy_to_user(ubuf, buffer, len) != 0) return -EFAULT;
    *ppos = len;
    return len;
}


static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = proc_read,
};

void save(unsigned int saddr, unsigned int daddr) {

    char tmp1[128];
    char tmp2[128];

    snprintf(tmp1, sizeof(tmp1), "Captured IP packet, saddr: %d.%d.%d.%d\n",
    ntohl(saddr) >> 24, (ntohl(saddr) >> 16) & 0x00FF,
    (ntohl(saddr) >> 8) & 0x0000FF, (ntohl(saddr)) & 0x000000FF);
    printk(KERN_INFO "saddr: %d.%d.%d.%d\n", ntohl(saddr) >> 24, (ntohl(saddr) >> 16) & 0x00FF,
    (ntohl(saddr) >> 8) & 0x0000FF, (ntohl(saddr)) & 0x000000FF);
    
    snprintf(tmp2, sizeof(tmp2), "daddr: %d.%d.%d.%d\n",
    ntohl(daddr) >> 24, (ntohl(daddr) >> 16) & 0x00FF,
    (ntohl(daddr) >> 8) & 0x0000FF, (ntohl(daddr)) & 0x000000FF);
    printk(KERN_INFO "daddr: %d.%d.%d.%d\n", ntohl(daddr) >> 24, (ntohl(daddr) >> 16) & 0x00FF,
    (ntohl(daddr) >> 8) & 0x0000FF, (ntohl(daddr)) & 0x000000FF);

    int len = strlen(tmp1) + strlen(tmp2);

    if (len > BUFFSIZE - buffer_ptr)
    {
        printk(KERN_INFO "Driver: Result buffer is full, dumping it to kernel log to avoid overflow\n");
        printk(KERN_INFO "%s\n", buffer);

        kfree(buffer);
        init_buffer();
    }

    buffer_ptr += (sprintf(buffer + buffer_ptr,"%s", tmp1));
    buffer_ptr += (sprintf(buffer + buffer_ptr,"%s\n", tmp2));

}

static char check_frame(struct sk_buff *skb) {
    struct iphdr *ip = (struct iphdr *)skb_network_header(skb);
    int ip_frag1 = ntohl(ip->daddr) >> 24;
    int ip_frag2 = ntohl(ip->daddr) >> 16 & 0x00FF;
    int ip_frag3 = ntohl(ip->daddr)>> 8 & 0x0000FF;
    int ip_frag4 = ntohl(ip->daddr) & 0x000000FF;
    //printk(KERN_INFO "daddr: %d.%d.%d.%d\n", ip_frag1, ip_frag2, ip_frag3, ip_frag4);

    if (ip_frag1 != 127 || ip_frag2 != 0 || ip_frag3 != 0 || ip_frag4 != 53) {
        dropped_packets++;
        return 0;
    }
    processed_packets++;

    save(ip->saddr, ip->daddr);
    return 1;
}

static rx_handler_result_t handle_frame(struct sk_buff **pskb) {

    if (check_frame(*pskb)) {
        stats.rx_packets++;
        stats.rx_bytes += (*pskb)->len;
    }
    (*pskb)->dev = child;
    return RX_HANDLER_ANOTHER;
}

static int open(struct net_device *dev) {
    netif_start_queue(dev);
    printk(KERN_INFO "%s: device opened", dev->name);
    return 0;
}

static int stop(struct net_device *dev) {
    netif_stop_queue(dev);
    printk(KERN_INFO "%s: device closed", dev->name);
    if (entry) proc_remove(entry);
        return 0;
}

static netdev_tx_t start_xmit(struct sk_buff *skb, struct net_device *dev) {
    struct priv *priv = netdev_priv(dev);

    if (check_frame(skb)) {
        stats.tx_packets++;
        stats.tx_bytes += skb->len;
    }

    if (priv->parent) {
        skb->dev = priv->parent;
        skb->priority = 1;
        dev_queue_xmit(skb);
        return 0;
    }
    return NETDEV_TX_OK;
}

static struct net_device_stats *get_stats(struct net_device *dev) {
    return &stats;
}

static struct net_device_ops crypto_net_device_ops = {
    .ndo_open = open,
    .ndo_stop = stop,
    .ndo_get_stats = get_stats,
    .ndo_start_xmit = start_xmit
};

static void setup(struct net_device *dev) {
    int i;
    ether_setup(dev);
    memset(netdev_priv(dev), 0, sizeof(struct priv));
    dev->netdev_ops = &crypto_net_device_ops;
    init_buffer();

    //fill in the MAC address with a phoney
    for (i = 0; i < ETH_ALEN; i++)
        dev->dev_addr[i] = (char)i;
}

int __init vni_init(void) {
    int err = 0;
    struct priv *priv;
    entry = proc_create("var2data", 0444, NULL, &fops);
    child = alloc_netdev(sizeof(struct priv), ifname, NET_NAME_UNKNOWN, setup);
    if (child == NULL) {
        printk(KERN_ERR "%s: allocate error", THIS_MODULE->name);
        return -ENOMEM;
    }
    priv = netdev_priv(child);
    priv->parent = __dev_get_by_name(&init_net, link); //parent interface
    if (!priv->parent) {
        printk(KERN_ERR "%s: no such net: %s", THIS_MODULE->name, link);
        free_netdev(child);
        return -ENODEV;
    }
    if (priv->parent->type != ARPHRD_ETHER && priv->parent->type != ARPHRD_LOOPBACK) {
        printk(KERN_ERR "%s: illegal net type", THIS_MODULE->name);
        free_netdev(child);
        return -EINVAL;
    }

    //copy IP, MAC and other information
    memcpy(child->dev_addr, priv->parent->dev_addr, ETH_ALEN);
    memcpy(child->broadcast, priv->parent->broadcast, ETH_ALEN);
    if ((err = dev_alloc_name(child, child->name))) {
        printk(KERN_ERR "%s: allocate name, error %i", THIS_MODULE->name, err);
        free_netdev(child);
        return -EIO;
    }

    register_netdev(child);
    rtnl_lock();
    netdev_rx_handler_register(priv->parent, &handle_frame, NULL);
    rtnl_unlock();
    printk(KERN_INFO "Module %s loaded", THIS_MODULE->name);
    printk(KERN_INFO "%s: create link %s", THIS_MODULE->name, child->name);
    printk(KERN_INFO "%s: registered rx handler for %s", THIS_MODULE->name, priv->parent->name);
    return 0;
}

void __exit vni_exit(void) {
    struct priv *priv = netdev_priv(child);
    if (priv->parent) {
        rtnl_lock();
        netdev_rx_handler_unregister(priv->parent);
        rtnl_unlock();
        printk(KERN_INFO "%s: unregister rx handler for %s", THIS_MODULE->name, priv->parent->name);
    }
    unregister_netdev(child);
    free_netdev(child);
    printk(KERN_INFO "Module %s unloaded", THIS_MODULE->name);
}

module_init(vni_init);
module_exit(vni_exit);
