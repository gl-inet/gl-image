#define pr_fmt(fmt) "gl-mpflow: " fmt

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/netdevice.h>
#include <linux/proc_fs.h>
#include "common.h"

#define EXPIRES    HZ
#define BUFSIZE    256

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 14, 336)
static void update_agg_traffic(struct timer_list *timer);
static DEFINE_TIMER(traffic_timer, update_agg_traffic);
static void regular_cleaning(struct timer_list *timer);
static DEFINE_TIMER(deading_timer, regular_cleaning);
#else
static void update_agg_traffic(unsigned long param);
static void regular_cleaning(unsigned long param);
static DEFINE_TIMER(traffic_timer, update_agg_traffic, 0, 0);
static DEFINE_TIMER(deading_timer, regular_cleaning, 0, 0);
#endif

static int gl_time = 0;
static bool need_shift = false;

static void update_mdev_info(void)
{
    struct gl_monitor_dev *mdev;

    write_lock(&mpflow_lock);
    list_for_each_entry(mdev, &mpflow_active, list) {
        int i;
        for (i = 0; i + 1 < STAT_TIME; i++) {
            mdev->rx_rate[i] = mdev->rx_rate[i + 1];
            mdev->tx_rate[i] = mdev->tx_rate[i + 1];
        }
        mdev->rx_rate[i] = 0;
        mdev->tx_rate[i] = 0;
    }
    write_unlock(&mpflow_lock);

    write_lock(&mpdead_lock);
    list_for_each_entry(mdev, &mpflow_deading, list) {
        int i;
        for (i = 0; i + 1 < STAT_TIME; i++) {
            mdev->rx_rate[i] = mdev->rx_rate[i + 1];
            mdev->tx_rate[i] = mdev->tx_rate[i + 1];
        }
        mdev->rx_rate[i] = 0;
        mdev->tx_rate[i] = 0;
    }
    write_unlock(&mpdead_lock);
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 14, 336)
static void regular_cleaning(struct timer_list *timer)
#else
static void regular_cleaning(unsigned long param)
#endif
{
    struct gl_monitor_dev *mdev, *n;
    time64_t t = ktime_get_boottime_seconds();

    write_lock(&mpdead_lock);
    list_for_each_entry_safe(mdev, n,   &mpflow_deading, list) {
        if (mdev->time + STAT_TIME * 60 > t)
            continue;

        list_del(&mdev->list);
        kfree(mdev);
    }
    write_unlock(&mpdead_lock);

    mod_timer(&deading_timer, jiffies + 60 * HZ);
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 14, 336)
static void update_agg_traffic(struct timer_list *timer)
#else
static void update_agg_traffic(unsigned long param)
#endif
{
    struct rtnl_link_stats64 storage;
    struct gl_monitor_dev *mdev;
    static int time_second = 0;
    static int time_min = 0;
    struct net_device *netdev;

    gl_time = need_shift ? STAT_TIME - 1 : time_min;

    write_lock(&mpflow_lock);
    list_for_each_entry(mdev,   &mpflow_active, list) {
        unsigned long long last_rx, last_tx;
        netdev = dev_get_by_name(&init_net, mdev->netdev);
        if (!netdev)
            continue;
        if (!need_statistics(netdev)) {
            clean_rate_flag(mdev);
            dev_put(netdev);
            continue;
        }
        dev_get_stats(netdev, &storage);
        dev_put(netdev);

        if (mdev->flag & MDEV_RX) {
            last_rx = mdev->rx_bytes;
            mdev->rx_bytes = storage.rx_bytes;
            mdev->rx_rate[gl_time] += mdev->rx_bytes - last_rx;
        } else {
            mdev->flag |= MDEV_RX;
            mdev->rx_bytes = storage.rx_bytes;
        }

        if (mdev->flag & MDEV_TX) {
            last_tx = mdev->tx_bytes;
            mdev->tx_bytes = storage.tx_bytes;
            mdev->tx_rate[gl_time] += mdev->tx_bytes - last_tx;
        } else {
            mdev->flag |= MDEV_TX;
            mdev->tx_bytes = storage.tx_bytes;
        }
    }
    write_unlock(&mpflow_lock);

    time_second += 1;
    if (time_second >= 60) {
        time_second = 0;
        time_min = time_min + 1;
        if (time_min >= STAT_TIME) {
            time_min = 0;
            need_shift = true;
        }
        if (need_shift)
            update_mdev_info();
    }

    mod_timer(&traffic_timer, jiffies + EXPIRES);
}

static int proc_show(struct seq_file *s, void *v)
{
    return 0;
}

static int proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, proc_show, NULL);
}

static ssize_t proc_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
    char tmp_buf[BUFSIZE + 1];

    memset(tmp_buf, 0x0, sizeof(tmp_buf));
    size = size > BUFSIZE ? BUFSIZE : size;
    if (*ppos > 0 || copy_from_user(tmp_buf, buf, size)) {
        pr_err("write error.\n");
        return -1;
    }

    *ppos = size;

    parse_str(tmp_buf);
    return size;
}

static int active_rx_show(struct seq_file *s, void *v)
{
    struct gl_monitor_dev *mdev;
    bool first = false;
    int i;

    read_lock_bh(&mpflow_lock);
    list_for_each_entry(mdev,   &mpflow_active, list) {
        if (!first) {
            first = true;
            seq_printf(s, "%-6d%-13s", gl_time, mdev->netdev);
        } else {
            seq_printf(s, "%-13s", mdev->netdev);
        }
    }

    if (first)
        seq_printf(s, "\n");
    else {
        read_unlock_bh(&mpflow_lock);
        return 0;
    }

    for (i = 0; i < STAT_TIME; i++) {
        if (i < 10)
            seq_printf(s, "[0%d]  ", i);
        else
            seq_printf(s, "[%d]  ", i);

        list_for_each_entry(mdev,   &mpflow_active, list) {
            seq_printf(s, "%-13llu", mdev->rx_rate[i]);
        }
        seq_printf(s, "\n");
    }

    read_unlock_bh(&mpflow_lock);

    return 0;
}

static int active_rx_open(struct inode *inode, struct file *file)
{
    return single_open(file, active_rx_show, NULL);
}

static int active_tx_show(struct seq_file *s, void *v)
{


    struct gl_monitor_dev *mdev;
    bool first = false;
    int i;


    read_lock_bh(&mpflow_lock);
    list_for_each_entry(mdev,   &mpflow_active, list) {
        if (!first) {
            first = true;
            seq_printf(s, "%-6d%-13s", gl_time, mdev->netdev);
        } else {
            seq_printf(s, "%-13s", mdev->netdev);
        }
    }

    if (first)
        seq_printf(s, "\n");
    else {
        read_unlock_bh(&mpflow_lock);
        return 0;
    }

    for (i = 0; i < STAT_TIME; i++) {
        if (i < 10)
            seq_printf(s, "[0%d]  ", i);
        else
            seq_printf(s, "[%d]  ", i);

        list_for_each_entry(mdev,   &mpflow_active, list) {
            seq_printf(s, "%-13llu", mdev->tx_rate[i]);
        }
        seq_printf(s, "\n");
    }

    read_unlock_bh(&mpflow_lock);

    return 0;
}

static int active_tx_open(struct inode *inode, struct file *file)
{
    return single_open(file, active_tx_show, NULL);
}

static int deading_rx_show(struct seq_file *s, void *v)
{
    struct gl_monitor_dev *mdev;
    bool first = false;
    int i;

    read_lock_bh(&mpdead_lock);
    list_for_each_entry(mdev,   &mpflow_deading, list) {
        if (!first) {
            first = true;
            seq_printf(s, "%-6d%-13s", gl_time, mdev->netdev);
        } else {
            seq_printf(s, "%-13s", mdev->netdev);
        }
    }

    if (first)
        seq_printf(s, "\n");
    else {
        read_unlock_bh(&mpdead_lock);
        return 0;
    }

    for (i = 0; i < STAT_TIME; i++) {
        if (i < 10)
            seq_printf(s, "[0%d]  ", i);
        else
            seq_printf(s, "[%d]  ", i);

        list_for_each_entry(mdev,   &mpflow_deading, list) {
            seq_printf(s, "%-13llu", mdev->rx_rate[i]);
        }
        seq_printf(s, "\n");
    }

    read_unlock_bh(&mpdead_lock);

    return 0;
}

static int deading_rx_open(struct inode *inode, struct file *file)
{
    return single_open(file, deading_rx_show, NULL);
}

static int deading_tx_show(struct seq_file *s, void *v)
{
    struct gl_monitor_dev *mdev;
    bool first = false;
    int i;

    read_lock_bh(&mpdead_lock);
    list_for_each_entry(mdev,   &mpflow_deading, list) {
        if (!first) {
            first = true;
            seq_printf(s, "%-6d%-13s", gl_time, mdev->netdev);
        } else {
            seq_printf(s, "%-13s", mdev->netdev);
        }
    }

    if (first)
        seq_printf(s, "\n");
    else {
        read_unlock_bh(&mpdead_lock);
        return 0;
    }

    for (i = 0; i < STAT_TIME; i++) {
        if (i < 10)
            seq_printf(s, "[0%d]  ", i);
        else
            seq_printf(s, "[%d]  ", i);

        list_for_each_entry(mdev,   &mpflow_deading, list) {
            seq_printf(s, "%-13llu", mdev->tx_rate[i]);
        }
        seq_printf(s, "\n");
    }

    read_unlock_bh(&mpdead_lock);

    return 0;
}

static int deading_tx_open(struct inode *inode, struct file *file)
{
    return single_open(file, deading_tx_show, NULL);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
const static struct file_operations gl_proc_ops = {
    .owner      = THIS_MODULE,
    .open       = proc_open,
    .read       = seq_read,
    .write      = proc_write,
    .llseek     = seq_lseek,
    .release    = single_release,
};
const static struct file_operations gl_active_tx_ops = {
    .owner      = THIS_MODULE,
    .open       = active_tx_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
const static struct file_operations gl_active_rx_ops = {
    .owner      = THIS_MODULE,
    .open       = active_rx_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
const static struct file_operations gl_deading_tx_ops = {
    .owner      = THIS_MODULE,
    .open       = deading_tx_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
const static struct file_operations gl_deading_rx_ops = {
    .owner      = THIS_MODULE,
    .open       = deading_rx_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};
#else
const static struct proc_ops gl_proc_ops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_open       = proc_open,
    .proc_read       = seq_read,
    .proc_write      = proc_write,
    .proc_lseek      = seq_lseek,
    .proc_release    = single_release,
};
const static struct proc_ops gl_active_tx_ops = {
    .proc_flags      = PROC_ENTRY_PERMANENT,
    .proc_open       = active_tx_open,
    .proc_read       = seq_read,
    .proc_lseek      = seq_lseek,
    .proc_release    = single_release,
};
const static struct proc_ops gl_active_rx_ops = {
    .proc_flags      = PROC_ENTRY_PERMANENT,
    .proc_open       = active_rx_open,
    .proc_read       = seq_read,
    .proc_lseek      = seq_lseek,
    .proc_release    = single_release,
};
const static struct proc_ops gl_deading_tx_ops = {
    .proc_flags      = PROC_ENTRY_PERMANENT,
    .proc_open       = deading_tx_open,
    .proc_read       = seq_read,
    .proc_lseek      = seq_lseek,
    .proc_release    = single_release,
};
const static struct proc_ops gl_deading_rx_ops = {
    .proc_flags      = PROC_ENTRY_PERMANENT,
    .proc_open       = deading_rx_open,
    .proc_read       = seq_read,
    .proc_lseek      = seq_lseek,
    .proc_release    = single_release,
};
#endif

static int __init mpflow_statistics_init(void)
{
    struct proc_dir_entry *proc;

    proc = proc_mkdir("gl-mpflow", NULL);
    if (!proc) {
        pr_err("can't create dir /proc/gl-mpflow/\n");
        return -ENODEV;;
    }
    proc_create("control", 0644, proc, &gl_proc_ops);
    proc_create("active_tx", 0444, proc, &gl_active_tx_ops);
    proc_create("active_rx", 0444, proc, &gl_active_rx_ops);
    proc_create("deading_tx", 0444, proc, &gl_deading_tx_ops);
    proc_create("deading_rx", 0444, proc, &gl_deading_rx_ops);

    mod_timer(&traffic_timer, jiffies + EXPIRES);
    mod_timer(&deading_timer, jiffies + EXPIRES);
    return 0;
}

static void __exit mpflow_statistics_exit(void)
{
    del_timer(&traffic_timer);
    del_timer(&deading_timer);
    flush_monitor_dev();
    remove_proc_subtree("gl-mpflow", NULL);
}

module_init(mpflow_statistics_init);
module_exit(mpflow_statistics_exit);
MODULE_LICENSE("GPL");
