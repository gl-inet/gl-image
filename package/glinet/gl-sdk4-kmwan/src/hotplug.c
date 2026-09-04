#include <linux/workqueue.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/if.h>
#include "kmwan.h"

#define BH_SKB_SIZE 1024

static int kmwan_event_add_var(struct sk_buff *skb, int argv,
                               const char *format, ...)
{
    char buf[128];
    char *s;
    va_list args;
    int len;

    if (argv)
        return -1;

    va_start(args, format);
    len = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    if (len >= sizeof(buf)) {
        //printk("[%s %d] OOM!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    s = skb_put(skb, len + 1);
    strncpy(s, buf, sizeof(buf) - 1);

    return 0;
}

static void hotplug_work(struct work_struct *work)
{
    struct gl_netcell *event_node = container_of(work, struct gl_netcell, work);
    struct sk_buff *skb = alloc_skb(BH_SKB_SIZE, GFP_KERNEL);
    int need_free;

    if (!skb)
        return;

    if (!event_node) {
        kfree_skb(skb);
        return;
    }

    need_free = kmwan_event_add_var(skb, 0, "SUBSYSTEM=%s", "kmwan");
    if (need_free)
        goto out_free_skb;

    need_free = kmwan_event_add_var(skb, 0, "STATUS=%s", event_node->state == DEAD ? "offline" : "online");
    if (need_free)
        goto out_free_skb;

    kmwan_event_add_var(skb, 0, "INTERFACE=%s", event_node->interface);
    if (need_free)
        goto out_free_skb;

    NETLINK_CB(skb).dst_group = 1;
    broadcast_uevent(skb, 0, 1, GFP_KERNEL);

out_free_skb:
    if (need_free)
        kfree_skb(skb);

    return;
}

void kmwan_hotplug(struct gl_netcell *node)
{
    schedule_work(&node->work);
}

void kmwan_hotplug_init(struct gl_netcell *node)
{
    INIT_WORK(&node->work, hotplug_work);
}
