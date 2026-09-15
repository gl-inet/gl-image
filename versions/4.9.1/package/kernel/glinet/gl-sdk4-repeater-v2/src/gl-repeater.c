#include <linux/module.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/netfilter_ipv4.h>

struct gl_repeater {
    char ifname[IFNAMSIZ];
    int ifindex;
} gl_repeater;

static inline void ip_increase_ttl(struct gl_repeater *r, struct iphdr *iph,
                                   struct net_device *dev)
{
    u32 check;

    if (likely(dev->ifindex != r->ifindex))
        return;

    if (iph->ttl == 255)
        return;

    check = (__force u32)iph->check;
    check += (__force u32)htons(0XFEFF);
    iph->check = (__force __sum16)(check + (check >= 0xFFFF));
    iph->ttl++;
}

static unsigned int gl_prerouting_hook(void *priv,
                                       struct sk_buff *skb,
                                       const struct nf_hook_state *state)
{
    struct iphdr *iph = ip_hdr(skb);

    ip_increase_ttl(&gl_repeater, iph, state->in);

    return NF_ACCEPT;
}

static unsigned int gl_postrouting_hook(void *priv,
                                        struct sk_buff *skb,
                                        const struct nf_hook_state *state)
{
    struct iphdr *iph = ip_hdr(skb);

    /* skip from local */
    if (unlikely(skb->sk))
        return NF_ACCEPT;

    ip_increase_ttl(&gl_repeater, iph, state->out);

    return NF_ACCEPT;
}

static inline void ip6_increase_ttl(struct gl_repeater *r, struct ipv6hdr *hdr,
                                    struct net_device *dev)
{
    if (likely(dev->ifindex != r->ifindex))
        return;

    if (hdr->hop_limit == 255)
        return;

    hdr->hop_limit++;
}

static unsigned int gl_prerouting_hook6(void *priv,
                                        struct sk_buff *skb,
                                        const struct nf_hook_state *state)
{
    struct ipv6hdr *hdr = ipv6_hdr(skb);

    ip6_increase_ttl(&gl_repeater, hdr, state->in);

    return NF_ACCEPT;
}

static unsigned int gl_postrouting_hook6(void *priv,
        struct sk_buff *skb,
        const struct nf_hook_state *state)
{
    struct ipv6hdr *hdr = ipv6_hdr(skb);

    /* skip from local */
    if (unlikely(skb->sk))
        return NF_ACCEPT;

    ip6_increase_ttl(&gl_repeater, hdr, state->out);

    return NF_ACCEPT;
}

static struct nf_hook_ops nf_hooks[] __read_mostly = {
    {
        .hook       = gl_prerouting_hook,
        .pf         = PF_INET,
        .hooknum    = NF_INET_PRE_ROUTING,
        .priority   = NF_IP_PRI_MANGLE
    },
    {
        .hook       = gl_prerouting_hook6,
        .pf         = PF_INET6,
        .hooknum    = NF_INET_PRE_ROUTING,
        .priority   = NF_IP_PRI_MANGLE
    },
    {
        .hook       = gl_postrouting_hook,
        .pf         = PF_INET,
        .hooknum    = NF_INET_POST_ROUTING,
        .priority   = NF_IP_PRI_MANGLE
    },
    {
        .hook       = gl_postrouting_hook6,
        .pf         = PF_INET6,
        .hooknum    = NF_INET_POST_ROUTING,
        .priority   = NF_IP_PRI_MANGLE
    }
};

static int proc_show(struct seq_file *s, void *v)
{
    seq_printf(s, "ifname: %s\n", gl_repeater.ifname);
    seq_printf(s, "ifindex: %d\n", gl_repeater.ifindex);

    return 0;
}

static ssize_t proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char *value, *newline;
    char conf[128] = "";

    if (size > sizeof(conf) - 1)
        return -EINVAL;

    if (copy_from_user(conf, buf, size))
        return -EFAULT;

    value = strchr(conf, '=');
    if (!value)
        return -EINVAL;
    *value++ = 0;

    newline = strchr(value, '\n');
    if (newline)
        *newline = '\0';

    if (!strcmp(conf, "ifname")) {
        struct net_device *dev;

        if (strlen(value) >= IFNAMSIZ)
            return -EINVAL;

        if (value[0] == '\0') {
            rtnl_lock();
            gl_repeater.ifname[0] = '\0';
            gl_repeater.ifindex = 0;
            rtnl_unlock();
            return size;
        }

        rtnl_lock();
        strncpy(gl_repeater.ifname, value, sizeof(gl_repeater.ifname));
        rtnl_unlock();

        dev = dev_get_by_name(&init_net, value);
        if (!dev)
            return size;

        gl_repeater.ifindex = dev->ifindex;

        dev_put(dev);
    } else {
        return -EINVAL;
    }

    return size;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
static const struct file_operations gl_proc_ops = {
    .owner      = THIS_MODULE,
    .open       = proc_open,
    .read       = seq_read,
    .write      = proc_write,
    .llseek     = seq_lseek,
    .release    = single_release
};
#else
static const struct proc_ops gl_proc_ops = {
    .proc_open       = proc_open,
    .proc_read       = seq_read,
    .proc_write      = proc_write,
    .proc_lseek     = seq_lseek,
    .proc_release    = single_release
};
#endif

static int gl_device_event(struct notifier_block *unused,
                           unsigned long event, void *ptr)
{
    struct net_device *dev = netdev_notifier_info_to_dev(ptr);

    if (strcmp(netdev_name(dev), gl_repeater.ifname))
        return NOTIFY_DONE;

    switch (event) {
        case NETDEV_UNREGISTER:
            gl_repeater.ifindex = 0;
            break;
        case NETDEV_REGISTER:
            gl_repeater.ifindex = dev->ifindex;
            break;
    }

    return NOTIFY_DONE;
}

static struct notifier_block gl_notifier_block = {
    .notifier_call = gl_device_event,
};

static int __init gl_repeater_init(void)
{
    struct proc_dir_entry *proc;
    int ret;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 12, 14)
    ret = nf_register_net_hooks(&init_net, nf_hooks, ARRAY_SIZE(nf_hooks));
#else
    ret = nf_register_hooks(nf_hooks, ARRAY_SIZE(nf_hooks));
#endif
    if (ret < 0) {
        pr_err("can't register hook\n");
        return ret;
    }

    proc = proc_mkdir("gl-repeater", NULL);

    proc_create("config", 0644, proc, &gl_proc_ops);

    register_netdevice_notifier(&gl_notifier_block);

    // pr_info("gl-repeater: (C) 2024 jianhui zhao <jianhui.zhao@gl-inet.com>\n");

    return 0;
}

static void __exit gl_repeater_exit(void)
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 12, 14)
    nf_unregister_net_hooks(&init_net, nf_hooks, ARRAY_SIZE(nf_hooks));
#else
    nf_unregister_hooks(nf_hooks, ARRAY_SIZE(nf_hooks));
#endif

    remove_proc_subtree("gl-repeater", NULL);

    unregister_netdevice_notifier(&gl_notifier_block);
}

module_init(gl_repeater_init);
module_exit(gl_repeater_exit);

MODULE_AUTHOR("jianhui zhao <jianhui.zhao@gl-inet.com>");
MODULE_LICENSE("GPL");
