#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/string.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter_bridge.h>
#include <../net/bridge/br_private.h>

#include "black_white_list.h"

#define GL_MESH_IFACE_MAX_LEN   256
#define MESH_IFINDEX_MAX        16

static struct hlist_head mac_index[MAC_HASHENTRIES];
static short gl_black_white_sw  = !DISABLE;
static short gl_black_white_mode = BLACK;
static short black_white_debug_sw = 0;
static unsigned char gl_net_secure = 0;
static int brlan_index;
static int brguest_index;
static int briot_index;
static char gl_mesh_ifaces_str[GL_MESH_IFACE_MAX_LEN] = {0};
static int gl_mesh_ifindices[MESH_IFINDEX_MAX];
static int gl_mesh_ifcount;  /* 0 = mesh not configured, fast-path skip */

static DEFINE_SPINLOCK(lock);
static DEFINE_SPINLOCK(net_secure_lock);
static DEFINE_SPINLOCK(mesh_iface_lock);
static DEFINE_SPINLOCK(ifidx_lock);

#define BW_DEBUG(fmt, ...) do{                 \
    if (black_white_debug_sw == 1)            \
        printk(fmt,##__VA_ARGS__);            \
}while(0)

static void print_cmd_format(void)
{
    printk("===============black_white_list cmd==================\n");
    printk("Usage: echo [<cmd>] [<arg>] > /proc/black_white_list/control\n");
    printk("[<cmd>]\t\t<arg>\t\t\t<description>\n");
    printk("add\t\tmac_addr\t\tadd mac_addr to   list\n");
    printk("del\t\tmac_addr\t\tdel mac_addr from list\n");
    printk("clr\t\t\t\t\tdel all mac_addr in list\n");
    printk("black/white\t\t\t\t set mode\n");
}

static void parse_cmd(const char *proc_str, int *op)
{
    int len = 0;

    if (!proc_str) {
        BW_DEBUG("[%s %d] proc_str is null.\n", __FUNCTION__, __LINE__);
        return;
    }

    len = strlen(proc_str);
    if (len == ENALBE_LEN || len == DISABLE_LEN) {
        //the web interface does not have a master switch, the default is enable
        if (!memcmp(proc_str, "enable", ENALBE_LEN)) {
            *op = LIST_SW_SET;
            gl_black_white_sw = !DISABLE;
        }
        if (!memcmp(proc_str, "disable", DISABLE_LEN)) {
            *op = LIST_SW_SET;
            gl_black_white_sw = DISABLE;
        }
    } else if (len == MODE_LEN) {
        if (!memcmp(proc_str, "black", MODE_LEN)) {
            *op = LIST_MODE_SET;
            gl_black_white_mode = BLACK;
        }
        if (!memcmp(proc_str, "white", MODE_LEN)) {
            *op = LIST_MODE_SET;
            gl_black_white_mode = WHITE;
        }
    } else if (len == CMD_LEN) {
        if (!memcmp(proc_str, "clr", CMD_LEN))
            *op = LIST_CLR;
    } else if (len == CMD_LEN + 1 + STRING_MAC_LEN) {
        if (!memcmp(proc_str, "add", CMD_LEN))
            *op = (proc_str[CMD_LEN] == ' ' ? LIST_ADD : *op);

        if (!memcmp(proc_str, "del", CMD_LEN))
            *op = (proc_str[CMD_LEN] == ' ' ? LIST_DEL : *op);
    }
}

static void bw_list_rcu_free(struct rcu_head *head)
{
    struct dev_info *dev = container_of(head, struct dev_info, rcu);
    kfree(dev);
}

static bool is_valid_mac_addr(const char *mac)
{
    int i;
    int mac_addr_len = 0;
    //the space needs to be slightly larger than the mac string space to prevent situations like 12:34:56:78:12:1234
    char tmp_mac[20];
    char *sep = &tmp_mac[0];
    char *p = NULL, *tok = ":";

    memset(tmp_mac, 0x0, sizeof(tmp_mac));

    if (!mac) {
        BW_DEBUG("[%s %d]mac is null.\n", __FUNCTION__, __LINE__);
        return false;
    }

    memcpy(tmp_mac, mac, 19);
    p = strsep(&sep, tok);

    while (p != NULL) {
        if (strlen(p) != 2) {
            BW_DEBUG("[%s %d]mac format is error.\n", __FUNCTION__, __LINE__);
            return false;
        }

        for (i = 0; i < strlen(p); i++) {
            char *vaild_ch = "0123456789abcdefABCDEF";
            int valid_ch_index = 0;
            while (*vaild_ch != '\0') {
                if (p[i] == *vaild_ch)
                    break;

                vaild_ch++;
                valid_ch_index++;
            }
            if (valid_ch_index == strlen("0123456789abcdefABCDEF")) {
                BW_DEBUG("[%s %d]mac format is error.\n", __FUNCTION__, __LINE__);
                return false;
            }
        }
        mac_addr_len++;
        p = strsep(&sep, ":");
    }

    return mac_addr_len == ETH_ALEN ? true : false;
}

static u16 calc_mac_hash(const u8 *mac_arr)
{
    u16 ret;
    ret = (mac_arr[0] ^ mac_arr[5]) + (mac_arr[1] ^ mac_arr[4]) + (mac_arr[2] ^ mac_arr[3]);
    return ret % MAC_HASHENTRIES;
}

static struct dev_info *check_mac_in_hash(const u8 *mac)
{
    struct hlist_head *head = NULL;
    struct dev_info *dev = NULL;
    u16 hash_val = calc_mac_hash(mac);
    head = &mac_index[hash_val];

    rcu_read_lock();
    hlist_for_each_entry_rcu(dev, head, hlist) {
        if (!memcmp(mac, dev->mac, ETH_ALEN)) {
            rcu_read_unlock();
            return dev;
        }
    }
    rcu_read_unlock();

    return NULL;
}

static void add_black_white_list(const char *mac)
{
    u8 ret_mac[ETH_ALEN] = {0};
    u16 hash_val;
    struct dev_info *dev;

    if (!is_valid_mac_addr(mac))
        return;

    sscanf(mac, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
           &ret_mac[0], &ret_mac[1], &ret_mac[2], &ret_mac[3], &ret_mac[4], &ret_mac[5]);

    if (!is_valid_ether_addr(ret_mac)) {
        BW_DEBUG("[%s %d]multicase address or mac is 0 or mac is ff:ff:ff:ff:ff:ff\n", __FUNCTION__, __LINE__);
        return;
    }

    dev = check_mac_in_hash(ret_mac);
    if (dev) {
        BW_DEBUG("[%s %d]mac already exist.\n", __FUNCTION__, __LINE__);
        return;
    }

    hash_val = calc_mac_hash(ret_mac);
    dev = (struct dev_info *)kmalloc(sizeof(struct dev_info), GFP_KERNEL);
    if (!dev) {
        printk("[%s %d]kmalloc failed.\n", __FUNCTION__, __LINE__);
        return;
    }

    memcpy(dev->mac, ret_mac, ETH_ALEN);
    BW_DEBUG("[%s %d] add success! hash_val:%2d, mac:%pM\n", __FUNCTION__, __LINE__, hash_val, &dev->mac);
    spin_lock_bh(&lock);
    hlist_add_head_rcu(&dev->hlist, &mac_index[hash_val]);
    spin_unlock_bh(&lock);
}

static void del_black_white_list(const char *mac)
{
    u8 ret_mac[ETH_ALEN] = {0};
    struct dev_info *dev ;

    if (!is_valid_mac_addr(mac)) {
        BW_DEBUG("[%s %d]mac is invalid.\n", __FUNCTION__, __LINE__);
        return;
    }

    sscanf(mac, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
           &ret_mac[0], &ret_mac[1], &ret_mac[2], &ret_mac[3], &ret_mac[4], &ret_mac[5]);

    dev = check_mac_in_hash(ret_mac);
    if (!dev) {
        BW_DEBUG("[%s %d]not find this mac.\n", __FUNCTION__, __LINE__);
        return;
    }
    BW_DEBUG("[%s %d]delete success! mac:%pM\n", __FUNCTION__, __LINE__, &dev->mac);
    spin_lock_bh(&lock);
    hlist_del_rcu(&dev->hlist);
    spin_unlock_bh(&lock);
    call_rcu(&dev->rcu, bw_list_rcu_free);
}

static void clr_black_white_list(void)
{
    struct hlist_head *head = &mac_index[0];
    struct dev_info *dev = NULL;
    struct hlist_node *n;
    int i;

    for (i = 0; i < MAC_HASHENTRIES; i++) {
        spin_lock_bh(&lock);
        hlist_for_each_entry_safe(dev, n, head + i, hlist) {
            hlist_del_rcu(&dev->hlist);
            call_rcu(&dev->rcu, bw_list_rcu_free);
        }
        spin_unlock_bh(&lock);
    }
    rcu_barrier();
}

static void parse_proc_str(const char *proc_str)
{
    int op = LIST_UNDEF;

    parse_cmd(proc_str, &op);

    switch (op) {
        case LIST_ADD:
            //add 1 to skip space between command and string
            add_black_white_list(&proc_str[CMD_LEN + 1]);
            break;
        case LIST_DEL:
            del_black_white_list(&proc_str[CMD_LEN + 1]);
            break;
        case LIST_CLR:
            clr_black_white_list();
            break;
        case LIST_UNDEF:
            print_cmd_format();
            break;
        case LIST_MODE_SET:
        case LIST_SW_SET:
            break;
    }
}

static int list_proc_show(struct seq_file *s, void *v)
{
    struct dev_info *dev = NULL;
    struct hlist_head *head = &mac_index[0];
    struct hlist_node *n;
    int i, count = 1;

    spin_lock_bh(&lock);
    seq_printf(s, "SW : %s\n", gl_black_white_sw == DISABLE ? "disable" : "enable");
    seq_printf(s, "Mode  : %s\n", gl_black_white_mode == WHITE ? "white" : "black");
    seq_printf(s, "Debug : %hd\n", black_white_debug_sw);
    seq_printf(s, "ID\tMAC\n");
    for (i = 0; i < MAC_HASHENTRIES; i++) {
        if (!hlist_empty(&mac_index[i])) {
            hlist_for_each_entry_safe(dev, n, head + i, hlist) {
                seq_printf(s, "%d\t%pM", count++, dev->mac);
            }
            seq_printf(s, "\n");
        }
    }
    spin_unlock_bh(&lock);

    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, list_proc_show, NULL);
}

static ssize_t proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char tmp_buf[BUFSIZE + 1];
    char ch;

    memset(tmp_buf, 0x0, sizeof(tmp_buf));
    size = size > BUFSIZE ? BUFSIZE : size;
    if (*ppos > 0 || copy_from_user(tmp_buf, buf, size)) {
        printk("[%s %d] write error.\n", __FUNCTION__, __LINE__);
        return -1;
    }

    *ppos = size;
    ch = tmp_buf[size - 1];
    tmp_buf[size - 1] = (ch == '\n' ? '\0' : ch);//eliminate the carriage return of the echo command

    parse_proc_str(tmp_buf);

    return size;
}

static int debug_sw_show(struct seq_file *s, void *v)
{
    seq_printf(s, "debug:%d\n", black_white_debug_sw);
    return 0;
}

static int debug_open(struct inode *inode, struct file *file)
{
    return single_open(file, debug_sw_show, NULL);
}

static ssize_t debug_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char tmp_buf[BUFSIZE + 1];
    char ch;
    memset(tmp_buf, 0x0, sizeof(tmp_buf));
    size = size > BUFSIZE ? BUFSIZE : size;

    if (*ppos > 0 || copy_from_user(tmp_buf, buf, size)) {
        BW_DEBUG("[%s %d] write error.\n", __FUNCTION__, __LINE__);
        return -1;
    }

    *ppos = size;
    ch = tmp_buf[size - 1];
    tmp_buf[size - 1] = (ch == '\n' ? '\0' : ch);//eliminate the carriage return of the echo command

    if (strlen(tmp_buf) != 1)
        return -1;

    if (!memcmp(tmp_buf, "1", strlen("1"))) {
        black_white_debug_sw = 1;
        printk("debug switch is open.\n");
    }

    if (!memcmp(tmp_buf, "0", strlen("0"))) {
        black_white_debug_sw = 0;
        printk("debug switch is close.\n");
    }

    return size;
}

static int net_secure_show(struct seq_file *s, void *v)
{
    int lan_isolate_state = 0;
    int guest_isolate_state = 0;
    int iot_isolate_state = 0;
    int transfer_enable = 0;

    spin_lock_bh(&net_secure_lock);
    lan_isolate_state = !!(gl_net_secure & LAN_ISOLATE_SET);
    guest_isolate_state = !!(gl_net_secure & GUEST_ISOLATE_SET);
    iot_isolate_state = !!(gl_net_secure & IOT_ISOLATE_SET);
    transfer_enable = !!(gl_net_secure & LAN_TRANSFER_SET);
    spin_unlock_bh(&net_secure_lock);

    seq_printf(s, "lan_ap_isolate:%d\nguest_ap_isolate:%d\niot_ap_isolate:%d\nlan_transfer_enable:%d\n",
               lan_isolate_state, guest_isolate_state, iot_isolate_state, transfer_enable);
    return 0;
}

static int net_secure_open(struct inode *inode, struct file *file)
{
    return single_open(file, net_secure_show, NULL);
}

static ssize_t net_secure_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char tmp_buf[16];
    unsigned int net_state;
    int len;

    if (*ppos > 0)
        return -EINVAL;

    size = min(size, sizeof(tmp_buf) - 1);
    if (copy_from_user(tmp_buf, buf, size))
        return -EFAULT;

    tmp_buf[size] = '\0';

    len = strlen(tmp_buf);
    while (len > 0 && (tmp_buf[len - 1] == '\n' || tmp_buf[len - 1] == '\r' || tmp_buf[len - 1] == ' '))
        tmp_buf[--len] = '\0';
    while (*tmp_buf == ' ')
        memmove(tmp_buf, tmp_buf + 1, strlen(tmp_buf));
    if (len == 0)
        return -EINVAL;
    if (kstrtouint(tmp_buf, 10, &net_state)) {
        BW_DEBUG("[%s %d] invalid number: %s\n", __FUNCTION__, __LINE__, tmp_buf);
        return -EINVAL;
    }
    if (net_state > 255) {
        BW_DEBUG("[%s %d] value out of range: %u > 255\n", __FUNCTION__, __LINE__, net_state);
        return -EINVAL;
    }

    spin_lock_bh(&net_secure_lock);
    gl_net_secure = (unsigned char)net_state;
    spin_unlock_bh(&net_secure_lock);

    BW_DEBUG("net_secure set to: %u (0x%02x) -> "
             "lan_isolate:%d, guest_isolate:%d, iot_isolate:%d, transfer_enable:%d\n",
             gl_net_secure,
             gl_net_secure,
             !!(gl_net_secure & LAN_ISOLATE_SET),
             !!(gl_net_secure & GUEST_ISOLATE_SET),
             !!(gl_net_secure & IOT_ISOLATE_SET),
             !!(gl_net_secure & LAN_TRANSFER_SET));

    *ppos = size;
    return size;
}
/*
 * Check if devname starts with any of the comma-separated base names.
 * E.g., base "wlan14" matches "wlan14", "wlan14.sta1", "wlan14.sta2".
 */
static bool match_mesh_prefix(const char *devname, const char *bases, int bases_len)
{
    const char *p = bases;
    const char *end = bases + bases_len;

    while (p < end && *p) {
        const char *comma = strchr(p, ',');
        int plen = comma ? (int)(comma - p) : (int)strlen(p);

        if (plen > 0 && strncmp(devname, p, plen) == 0) {
            char next_ch = devname[plen];
            /* Exact match or sub-interface (e.g. ".sta1") */
            if (next_ch == '\0' || next_ch == '.')
                return true;
        }
        p += plen + (comma ? 1 : plen);
    }
    return false;
}

/*
 * Core logic: scan all net devices and rebuild mesh ifindex cache.
 * Caller must ensure rtnl is held (either via rtnl_lock or notifier context).
 */
static void __update_mesh_ifindex(void)
{
    char tmp[GL_MESH_IFACE_MAX_LEN];
    int new_indices[MESH_IFINDEX_MAX];
    int new_count = 0;
    struct net_device *dev;

    spin_lock_bh(&mesh_iface_lock);
    strncpy(tmp, gl_mesh_ifaces_str, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    spin_unlock_bh(&mesh_iface_lock);

    /* Empty string means mesh is not configured */
    if (!tmp[0])
        goto update;

    for_each_netdev(&init_net, dev) {
        if (new_count >= MESH_IFINDEX_MAX)
            break;
        if (match_mesh_prefix(dev->name, tmp, strlen(tmp))) {
            BW_DEBUG("[mesh] matched iface: %s (ifindex=%d)\n", dev->name, dev->ifindex);
            new_indices[new_count++] = dev->ifindex;
        }
    }

update:
    spin_lock_bh(&mesh_iface_lock);
    memcpy(gl_mesh_ifindices, new_indices, sizeof(int) * new_count);
    smp_wmb();
    WRITE_ONCE(gl_mesh_ifcount, new_count);
    spin_unlock_bh(&mesh_iface_lock);

    BW_DEBUG("[mesh] update_mesh_ifindex: bases=\"%s\", cached %d ifaces\n", tmp, new_count);
}

/*
 * Called from process context (e.g. proc write). Takes rtnl_lock itself.
 */
static void update_mesh_ifindex(void)
{
    rtnl_lock();
    __update_mesh_ifindex();
    rtnl_unlock();
}

static int mesh_iface_show(struct seq_file *s, void *v)
{
    spin_lock_bh(&mesh_iface_lock);
    seq_printf(s, "%s\n", gl_mesh_ifaces_str);
    spin_unlock_bh(&mesh_iface_lock);

    return 0;
}

static int mesh_iface_open(struct inode *inode, struct file *file)
{
    return single_open(file, mesh_iface_show, NULL);
}

static ssize_t mesh_iface_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char tmp[GL_MESH_IFACE_MAX_LEN];
    int len;

    if (*ppos > 0)
        return -EINVAL;

    size = min(size, sizeof(tmp) - 1);
    if (copy_from_user(tmp, buf, size))
        return -EFAULT;

    tmp[size] = '\0';

    len = strlen(tmp);
    while (len > 0 && (tmp[len - 1] == '\n' || tmp[len - 1] == '\r' || tmp[len - 1] == ' '))
        tmp[--len] = '\0';
    while (*tmp == ' ')
        memmove(tmp, tmp + 1, strlen(tmp));

    spin_lock_bh(&mesh_iface_lock);
    strncpy(gl_mesh_ifaces_str, tmp, GL_MESH_IFACE_MAX_LEN - 1);
    gl_mesh_ifaces_str[GL_MESH_IFACE_MAX_LEN - 1] = '\0';
    spin_unlock_bh(&mesh_iface_lock);

    update_mesh_ifindex();

    BW_DEBUG("[mesh] mesh_iface set to: \"%s\", ifcount: %d\n",
             gl_mesh_ifaces_str, READ_ONCE(gl_mesh_ifcount));

    *ppos = size;
    return size;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
static const struct file_operations gl_proc_ops = {
    .owner      = THIS_MODULE,
    .open       = proc_open,
    .read       = seq_read,
    .write      = proc_write,
    .llseek     = seq_lseek,
    .release    = single_release,
};
static const struct file_operations gl_debug_ops = {
    .owner      = THIS_MODULE,
    .open       = debug_open,
    .read       = seq_read,
    .write      = debug_write,
    .llseek     = seq_lseek,
    .release    = single_release,
};
static const struct file_operations gl_net_secure_ops = {
    .owner      = THIS_MODULE,
    .open       = net_secure_open,
    .read       = seq_read,
    .write      = net_secure_write,
    .llseek     = seq_lseek,
    .release    = single_release,
};
const static struct file_operations gl_mesh_iface_ops = {
    .owner      = THIS_MODULE,
    .open       = mesh_iface_open,
    .read       = seq_read,
    .write      = mesh_iface_write,
    .llseek     = seq_lseek,
    .release    = single_release,
};
#else
static const struct proc_ops gl_proc_ops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_open       = proc_open,
    .proc_read       = seq_read,
    .proc_write      = proc_write,
    .proc_lseek      = seq_lseek,
    .proc_release    = single_release,
};
static const struct proc_ops gl_debug_ops = {
    .proc_flags      = PROC_ENTRY_PERMANENT,
    .proc_open          = debug_open,
    .proc_read       = seq_read,
    .proc_write      = debug_write,
    .proc_lseek      = seq_lseek,
    .proc_release    = single_release,
};
static const struct proc_ops gl_net_secure_ops = {
    .proc_flags      = PROC_ENTRY_PERMANENT,
    .proc_open       = net_secure_open,
    .proc_read       = seq_read,
    .proc_write      = net_secure_write,
    .proc_lseek      = seq_lseek,
    .proc_release    = single_release,
};
const static struct proc_ops gl_mesh_iface_ops = {
    .proc_flags      = PROC_ENTRY_PERMANENT,
    .proc_open       = mesh_iface_open,
    .proc_read       = seq_read,
    .proc_write      = mesh_iface_write,
    .proc_lseek      = seq_lseek,
    .proc_release    = single_release,
};
#endif

static void hlist_init(void)
{
    int i;
    for (i = 0; i < MAC_HASHENTRIES; i++)
        INIT_HLIST_HEAD(&mac_index[i]);
}

static void black_white_list_proc_init(struct proc_dir_entry *proc)
{
    proc_create("control", 0644, proc, &gl_proc_ops);
    proc_create("list", 0400, proc, &gl_proc_ops);
    proc_create("debug", 0644, proc, &gl_debug_ops);
    proc_create("net_secure", 0644, proc, &gl_net_secure_ops);
    proc_create("mesh_iface", 0644, proc, &gl_mesh_iface_ops);
    hlist_init();
}

#if 0
static void print_hash(void)
{
    struct hlist_head *head = &mac_index[0];
    struct dev_info *dev = NULL;
    struct hlist_node *n;
    int i;

    spin_lock_bh(&lock);
    for (i = 0; i < MAC_HASHENTRIES; i++) {
        hlist_for_each_entry_safe(dev, n, head + i, hlist) {
            printk("[%s %d] hash_val:%2d, mac:%pM\n", __FUNCTION__, __LINE__, i, &dev->mac);
        }
    }
    spin_unlock_bh(&lock);
}
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static u32 black_white_list_bridge_pre_routing_hook(void *priv,
        struct sk_buff *skb,
        const struct nf_hook_state *state)
#else
static u32 black_white_list_bridge_pre_routing_hook(unsigned int hook,
        struct sk_buff *skb,
        const struct net_device *in,
        const struct net_device *out,
        int (*okfn)(struct sk_buff *))
#endif
{
    struct ethhdr *ehdr = eth_hdr(skb);

    if (gl_black_white_sw == DISABLE) {
        return NF_ACCEPT;
    }

    switch (gl_black_white_mode) {
        case BLACK:
            return (check_mac_in_hash(ehdr->h_source) || check_mac_in_hash(ehdr->h_dest)) ? NF_DROP : NF_ACCEPT;
        case WHITE:
            return (check_mac_in_hash(ehdr->h_source) || check_mac_in_hash(ehdr->h_dest)) ? NF_ACCEPT : NF_DROP;
        default:
            return NF_ACCEPT;
    }

}

static int get_ifindex_by_ifname(const char *ifname)
{
    struct net_device *dev = dev_get_by_name(&init_net, ifname);
    int ret = -1;
    if (dev) {
        ret = dev->ifindex;
        dev_put(dev);
    }
    return ret;
}



/* Fast-path mesh ifindex check, lock-free via READ_ONCE */
static bool is_mesh_ifindex(int ifindex)
{
    int i, cnt;

    cnt = READ_ONCE(gl_mesh_ifcount);
    if (cnt == 0)
        return false;

    smp_rmb();
    for (i = 0; i < cnt; i++) {
        if (READ_ONCE(gl_mesh_ifindices[i]) == ifindex)
            return true;
    }
    return false;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static u32 ap_isolate_bridge_forward_hook(void *priv,
        struct sk_buff *skb,
        const struct nf_hook_state *state)
#else
static u32 ap_isolate_bridge_forward_hook(unsigned int hook,
        struct sk_buff *skb,
        const struct net_device *in,
        const struct net_device *out,
        int (*okfn)(struct sk_buff *))
#endif
{
    struct net_device *br_dev = netdev_master_upper_dev_get_rcu(skb->dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
    const struct net_device *in  = state->in;
    const struct net_device *out = state->out;
#endif
    if (unlikely(!br_dev)) {
        return NF_ACCEPT;
    }

    BW_DEBUG("[%s %d]gl_net_secure:%x, br-lan:%d, br-guest:%d, br-iot:%d\n",
             __FUNCTION__, __LINE__, gl_net_secure, brlan_index, brguest_index, briot_index);

    spin_lock(&net_secure_lock);
    if (gl_net_secure & LAN_ISOLATE_SET) {
        if (brlan_index == br_dev->ifindex)
            goto drop;
    }
    if (gl_net_secure & GUEST_ISOLATE_SET) {
        if (brguest_index == br_dev->ifindex)
            goto drop;
    }
    if (gl_net_secure & IOT_ISOLATE_SET) {
        if (briot_index == br_dev->ifindex)
            goto drop;
    }

    spin_unlock(&net_secure_lock);
    return NF_ACCEPT;
drop:
    spin_unlock(&net_secure_lock);
    /* Only check mesh when packet would be dropped by isolation */
    if (is_mesh_ifindex(in->ifindex) || is_mesh_ifindex(out->ifindex)) {
        return NF_ACCEPT;
    }
    return NF_DROP;
}

static struct nf_hook_ops black_white_list_ops[] __read_mostly = {
    {
        .hook       = black_white_list_bridge_pre_routing_hook,
        .pf         = NFPROTO_BRIDGE,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
        .owner      = THIS_MODULE,
#endif
        .hooknum    = NF_BR_PRE_ROUTING,
        .priority   = NF_BR_PRI_FIRST,
    },
    {
        .hook       = ap_isolate_bridge_forward_hook,
        .pf         = NFPROTO_BRIDGE,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
        .owner      = THIS_MODULE,
#endif
        .hooknum    = NF_BR_FORWARD,
        .priority   = NF_BR_PRI_FIRST + 1,
    },
};

static void update_ifindex(void)
{
    spin_lock(&ifidx_lock);
    brlan_index = get_ifindex_by_ifname("br-lan");
    brguest_index = get_ifindex_by_ifname("br-guest");
    briot_index = get_ifindex_by_ifname("br-iot");
    spin_unlock(&ifidx_lock);
}

static int netseacure_device_event(struct notifier_block *unused,
                                   unsigned long event, void *ptr)
{
    switch (event) {
        case NETDEV_CHANGENAME:
        case NETDEV_UNREGISTER:
        case NETDEV_REGISTER:
            update_ifindex();
            __update_mesh_ifindex();
            break;
    }

    return NOTIFY_DONE;
}

static struct notifier_block netseacure_notifier_block = {
    .notifier_call = netseacure_device_event,
};

static int __init black_white_list_init(void)
{
    struct proc_dir_entry *proc;
    int ret = 0;

    proc = proc_mkdir("gl-black_white_list", NULL);
    if (!proc) {
        printk("Create /proc/gl-black_white_list failed.\n");
        return -1;
    }
    black_white_list_proc_init(proc);

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 12, 14)
    ret = nf_register_net_hooks(&init_net, black_white_list_ops, ARRAY_SIZE(black_white_list_ops));
#else
    ret = nf_register_hooks(black_white_list_ops, ARRAY_SIZE(black_white_list_ops));
#endif
    register_netdevice_notifier(&netseacure_notifier_block);
    if (ret < 0) {
        printk("[%s %d]can't register hook.\n", __FUNCTION__, __LINE__);
        return -1;
    }

    return 0;
}

static void __exit black_white_list_exit(void)
{
    unregister_netdevice_notifier(&netseacure_notifier_block);
    clr_black_white_list();
    remove_proc_subtree("gl-black_white_list", NULL);
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 12, 14)
    nf_unregister_net_hooks(&init_net, black_white_list_ops, ARRAY_SIZE(black_white_list_ops));
#else
    nf_unregister_hooks(black_white_list_ops, ARRAY_SIZE(black_white_list_ops));
#endif
}

module_init(black_white_list_init);
module_exit(black_white_list_exit);

MODULE_AUTHOR("yu zhang <yu.zhang@gl-inet.com>");
MODULE_LICENSE("GPL");
