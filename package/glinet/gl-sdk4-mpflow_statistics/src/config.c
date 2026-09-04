#define pr_fmt(fmt) "gl-mpflow: " fmt

#include <linux/string.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/netdevice.h>
#include "common.h"

#define MAX_LEN 128

DEFINE_RWLOCK(mpflow_lock);
DEFINE_RWLOCK(mpdead_lock);

LIST_HEAD(mpflow_active);
LIST_HEAD(mpflow_deading);

static struct gl_monitor_dev* find_active_mdev(struct net_device *dev)
{
    struct gl_monitor_dev *mdev;

    if (!strlen(dev->name))
        return NULL;

    read_lock_bh(&mpflow_lock);
    list_for_each_entry(mdev, &mpflow_active, list) {
        if (!strncmp(mdev->netdev, dev->name, IFNAMSIZ)) {
            read_unlock_bh(&mpflow_lock);
            return mdev;
        }
    }
    read_unlock_bh(&mpflow_lock);

    return NULL;
}

static struct gl_monitor_dev* find_deading_mdev(struct net_device *dev)
{
    struct gl_monitor_dev *mdev, *n;

    if (!strlen(dev->name))
        return NULL;

    read_lock_bh(&mpdead_lock);
    list_for_each_entry_safe(mdev, n, &mpflow_deading, list) {
        if (!strncmp(mdev->netdev, dev->name, IFNAMSIZ)) {
            write_lock_bh(&mpflow_lock);
            mdev->time = 0;
            list_move_tail(&mdev->list, &mpflow_active);
            write_unlock_bh(&mpflow_lock);
            read_unlock_bh(&mpdead_lock);
            return mdev;
        }
    }
    read_unlock_bh(&mpdead_lock);

    return NULL;
}

static bool create_node(struct net_device *netdev)
{
    struct gl_monitor_dev *mdev = kzalloc(sizeof(*mdev), GFP_ATOMIC);
    if (!mdev) {
        pr_err("Out of memory.\n");
        return false;
    }

    strcpy(mdev->netdev,netdev->name);
    write_lock_bh(&mpflow_lock);
    list_add_tail(&mdev->list, &mpflow_active);
    write_unlock_bh(&mpflow_lock);

    return true;
}

static void add_monitor_dev(char *buf)
{

    char *sep = buf + 1;
    char *p = NULL, *tok = " ";
    struct net_device *dev;

    if (strlen(buf) < 2) {
        pr_err("format err.\n");
        return;
    }

    p = strsep(&sep, tok);
    while (p) {
        dev = dev_get_by_name(&init_net, p);

        if (!dev ) {
            pr_err("'%s' is not find.\n", p);
            goto loop;
        }

        if(netif_is_bridge_master(dev) || netif_is_bridge_port(dev)) {
            pr_err("'%s' is bridge.\n", dev->name);
            goto loop;
        }

        if (!find_active_mdev(dev) &&
           !find_deading_mdev(dev) &&
           !create_node(dev)) {
            dev_put(dev);
            return; /*OOM,Direct return*/
        }
loop:
        p = strsep(&sep, tok);
        dev_put(dev);
    }

}

static void del_monitor_dev(char *buf)
{
    struct gl_monitor_dev *mdev;
    struct net_device *dev;
    char *sep = buf + 1;
    char *p = NULL, *tok = " ";

    if (strlen(buf) < 2) {
        pr_err("[%s] format err.\n", __FUNCTION__);
        return;
    }

    p = strsep(&sep, tok);
    while(p) {
        dev = dev_get_by_name(&init_net, p);
        if (!dev)
            goto loop;

        mdev = find_active_mdev(dev);
        if (!mdev)
            goto release_dev;

        write_lock_bh(&mpflow_lock);
        write_lock_bh(&mpdead_lock);
        clean_rate_flag(mdev);
        mdev->time = ktime_get_boottime_seconds();
        list_move_tail(&mdev->list, &mpflow_deading);
        write_unlock_bh(&mpdead_lock);
        write_unlock_bh(&mpflow_lock);

release_dev:
        dev_put(dev);
loop:
        p = strsep(&sep, tok);
    }
}

void flush_monitor_dev(void)
{
    struct gl_monitor_dev *mdev, *n;
    write_lock_bh(&mpflow_lock);
    list_for_each_entry_safe(mdev, n, &mpflow_active, list) {
        list_del(&mdev->list);
        kfree(mdev);
    }
    write_unlock_bh(&mpflow_lock);

    write_lock_bh(&mpdead_lock);
    list_for_each_entry_safe(mdev, n, &mpflow_deading, list) {
        list_del(&mdev->list);
        kfree(mdev);
    }
    write_unlock_bh(&mpdead_lock);
}

void parse_str(char *buf)
{
    if (!strlen(buf)) {
        pr_err("Null string.\n");
        return;
    }

    switch (buf[0]) {
        case 'a':
            add_monitor_dev(buf + 1);
            break;
        case 'd':
            del_monitor_dev(buf + 1);
            break;
        case 'c':
            flush_monitor_dev();
            break;
        default:
            pr_warn("'%c' is an undefined command.\n", buf[0]);
    }
}
