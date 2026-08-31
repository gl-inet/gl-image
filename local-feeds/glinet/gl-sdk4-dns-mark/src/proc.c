#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/version.h>
#include "proc.h"
#include "dns_mark.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,17,0))
#define PDE_DATA pde_data
#endif

struct dns_mark_proc_dir proc_dir;
extern struct dns_mark_rule_mgr rule_mgr;

/* Global variables */
char *dns_mark_write_domains_pending_buf = NULL;
size_t dns_mark_write_domains_pending_len = 0;

/* Recalculate whether any rule contains domain entries and update the cached flag. */
static void dns_mark_recalc_domain_rules_present(void)
{
    struct dns_mark_rule *rule;
    bool present = false;

    spin_lock(&rule_mgr.lock);
    list_for_each_entry(rule, &rule_mgr.rules, list) {
        if (atomic_read(&rule->domain_count) > 0) {
            present = true;
            break;
        }
    }
    spin_unlock(&rule_mgr.lock);

    atomic_set(&rule_mgr.domain_rules_present, present ? 1 : 0);
}

/* Forward declarations */
int dns_mark_rule_dump_show(struct seq_file *m, void *v);
int dns_mark_rule_dump_open(struct inode *inode, struct file *file);
int dns_mark_dump_show(struct seq_file *m, void *v);
int dns_mark_dump_open(struct inode *inode, struct file *file);
int dns_mark_ifnames_show(struct seq_file *m, void *v);
int dns_mark_ifnames_open(struct inode *inode, struct file *file);
int dns_mark_macs_show(struct seq_file *m, void *v);
int dns_mark_macs_open(struct inode *inode, struct file *file);
int dns_mark_domains_show(struct seq_file *m, void *v);
int dns_mark_domains_open(struct inode *inode, struct file *file);
int dns_mark_mark_show(struct seq_file *m, void *v);
int dns_mark_mark_open(struct inode *inode, struct file *file);
int dns_mark_mac_flag_show(struct seq_file *m, void *v);
int dns_mark_mac_flag_open(struct inode *inode, struct file *file);
int dns_mark_domain_flag_show(struct seq_file *m, void *v);
int dns_mark_domain_flag_open(struct inode *inode, struct file *file);
int dns_mark_debug_show(struct seq_file *m, void *v);
int dns_mark_debug_open(struct inode *inode, struct file *file);
ssize_t dns_mark_write_debug(struct file *file, const char __user *buffer,
                             size_t count, loff_t *ppos);
int dns_mark_intercept_forward_show(struct seq_file *m, void *v);
int dns_mark_intercept_forward_open(struct inode *inode, struct file *file);
ssize_t dns_mark_write_intercept_forward(struct file *file, const char __user *buffer,
                             size_t count, loff_t *ppos);
ssize_t dns_mark_write_clear(struct file *file, const char __user *buffer,
                             size_t count, loff_t *ppos);
ssize_t dns_mark_write_mac_flag(struct file *file, const char __user *buffer,
                                size_t count, loff_t *ppos);
ssize_t dns_mark_write_domain_flag(struct file *file, const char __user *buffer,
                                   size_t count, loff_t *ppos);
ssize_t dns_mark_write_mark(struct file *file, const char __user *buffer,
                            size_t count, loff_t *ppos);
ssize_t dns_mark_write_ifnames(struct file *file, const char __user *buffer,
                               size_t count, loff_t *ppos);
ssize_t dns_mark_write_macs(struct file *file, const char __user *buffer,
                            size_t count, loff_t *ppos);
ssize_t dns_mark_write_domains(struct file *file, const char __user *buffer,
                               size_t count, loff_t *ppos);
ssize_t dns_mark_write_create(struct file *file, const char __user *buffer,
                              size_t count, loff_t *ppos);
int dns_mark_create_rule_proc(struct dns_mark_rule *rule, int rule_id);
void dns_mark_remove_rule_proc(struct dns_mark_rule_proc *rule_proc);
int dns_mark_proc_init(void);
void dns_mark_proc_cleanup(void);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
const struct file_operations dns_mark_dump_ops = {
    .owner = THIS_MODULE,
    .open = dns_mark_dump_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

const struct file_operations dns_mark_clear_ops = {
    .owner = THIS_MODULE,
    .write = dns_mark_write_clear,
};

const struct file_operations dns_mark_mac_flag_ops = {
    .owner = THIS_MODULE,
    .open = dns_mark_mac_flag_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
    .write = dns_mark_write_mac_flag,
};

const struct file_operations dns_mark_domain_flag_ops = {
    .owner = THIS_MODULE,
    .open = dns_mark_domain_flag_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
    .write = dns_mark_write_domain_flag,
};

const struct file_operations dns_mark_mark_ops = {
    .owner = THIS_MODULE,
    .open = dns_mark_mark_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
    .write = dns_mark_write_mark,
};

const struct file_operations dns_mark_ifnames_ops = {
    .owner = THIS_MODULE,
    .open = dns_mark_ifnames_open,
    .read = seq_read,
    .write = dns_mark_write_ifnames,
    .llseek = seq_lseek,
    .release = single_release,
};

const struct file_operations dns_mark_macs_ops = {
    .owner = THIS_MODULE,
    .open = dns_mark_macs_open,
    .read = seq_read,
    .write = dns_mark_write_macs,
    .llseek = seq_lseek,
    .release = single_release,
};

const struct file_operations dns_mark_domains_ops = {
    .owner = THIS_MODULE,
    .open = dns_mark_domains_open,
    .read = seq_read,
    .write = dns_mark_write_domains,
    .llseek = seq_lseek,
    .release = single_release,
};

const struct file_operations dns_mark_create_ops = {
    .owner = THIS_MODULE,
    .write = dns_mark_write_create,
};

const struct file_operations dns_mark_rule_dump_ops = {
    .owner = THIS_MODULE,
    .open = dns_mark_rule_dump_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

const struct file_operations dns_mark_debug_ops = {
    .owner = THIS_MODULE,
    .open = dns_mark_debug_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
    .write = dns_mark_write_debug,
};

const struct file_operations dns_mark_intercept_forward_ops = {
    .owner = THIS_MODULE,
    .open = dns_mark_intercept_forward_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
    .write = dns_mark_write_intercept_forward,
};
#else
const struct proc_ops dns_mark_dump_ops = {
    .proc_open = dns_mark_dump_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

const struct proc_ops dns_mark_clear_ops = {
    .proc_write = dns_mark_write_clear,
};

const struct proc_ops dns_mark_mac_flag_ops = {
    .proc_open = dns_mark_mac_flag_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
    .proc_write = dns_mark_write_mac_flag,
};

const struct proc_ops dns_mark_domain_flag_ops = {
    .proc_open = dns_mark_domain_flag_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
    .proc_write = dns_mark_write_domain_flag,
};

const struct proc_ops dns_mark_mark_ops = {
    .proc_open = dns_mark_mark_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
    .proc_write = dns_mark_write_mark,
};

const struct proc_ops dns_mark_ifnames_ops = {
    .proc_open = dns_mark_ifnames_open,
    .proc_read = seq_read,
    .proc_write = dns_mark_write_ifnames,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

const struct proc_ops dns_mark_macs_ops = {
    .proc_open = dns_mark_macs_open,
    .proc_read = seq_read,
    .proc_write = dns_mark_write_macs,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

const struct proc_ops dns_mark_domains_ops = {
    .proc_open = dns_mark_domains_open,
    .proc_read = seq_read,
    .proc_write = dns_mark_write_domains,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

const struct proc_ops dns_mark_create_ops = {
    .proc_write = dns_mark_write_create,
};

const struct proc_ops dns_mark_rule_dump_ops = {
    .proc_open = dns_mark_rule_dump_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

const struct proc_ops dns_mark_debug_ops = {
    .proc_open = dns_mark_debug_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
    .proc_write = dns_mark_write_debug,
};

const struct proc_ops dns_mark_intercept_forward_ops = {
    .proc_open = dns_mark_intercept_forward_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
    .proc_write = dns_mark_write_intercept_forward,
};
#endif

/* Show single rule */
int dns_mark_rule_dump_show(struct seq_file *m, void *v)
{
    struct dns_mark_rule_proc *rule_proc = m->private;
    struct dns_mark_rule *rule = rule_proc->rule;
    struct dns_mark_ifname_entry *ifname_entry;
    struct dns_mark_mac_entry *mac_entry;
    struct dns_mark_domain_entry *domain_entry;

    seq_printf(m, "Rule %d:\n", rule->rule_id);
    seq_printf(m, "  Ifnames:\n");
    list_for_each_entry(ifname_entry, &rule->ifname_list, list) {
        seq_printf(m, "    %s\n", ifname_entry->ifname);
    }
    seq_printf(m, "  MACs (blacklist=%d):\n", !!(rule->flags & RULE_MAC_BLACKLIST));
    list_for_each_entry(mac_entry, &rule->mac_list, list) {
        seq_printf(m, "    %pM\n", mac_entry->mac);
    }
    seq_printf(m, "  Domains (blacklist=%d):\n", !!(rule->flags & RULE_DOMAIN_BLACKLIST));
    list_for_each_entry(domain_entry, &rule->domain_list, list) {
        seq_printf(m, "    %s\n", domain_entry->domain_pattern);
    }
    seq_printf(m, "  Mark: 0x%x\n", rule->mark);

    return 0;
}

int dns_mark_rule_dump_open(struct inode *inode, struct file *file)
{
    return single_open(file, dns_mark_rule_dump_show, PDE_DATA(inode));
}

/* Dump all rules */
int dns_mark_dump_show(struct seq_file *m, void *v)
{
    struct dns_mark_rule *rule;
    struct dns_mark_ifname_entry *ifname_entry;
    struct dns_mark_mac_entry *mac_entry;
    struct dns_mark_domain_entry *domain_entry;

    seq_puts(m, "DNS Mark Rules:\n");
    spin_lock(&rule_mgr.lock);
    list_for_each_entry(rule, &rule_mgr.rules, list) {
        seq_printf(m, "Rule %d:\n", rule->rule_id);
        seq_printf(m, "  Ifnames:\n");
        list_for_each_entry(ifname_entry, &rule->ifname_list, list) {
            seq_printf(m, "    %s\n", ifname_entry->ifname);
        }
        seq_printf(m, "  MACs (blacklist=%d):\n", !!(rule->flags & RULE_MAC_BLACKLIST));         // cppcheck-suppress uninitvar
        list_for_each_entry(mac_entry, &rule->mac_list, list) {
            seq_printf(m, "    %pM\n", mac_entry->mac);
        }
        seq_printf(m, "  Domains (blacklist=%d):\n", !!(rule->flags & RULE_DOMAIN_BLACKLIST));         // cppcheck-suppress uninitvar
        list_for_each_entry(domain_entry, &rule->domain_list, list) {
            seq_printf(m, "    %s\n", domain_entry->domain_pattern);
        }
        seq_printf(m, "  Mark: 0x%x\n", rule->mark);
    }
    spin_unlock(&rule_mgr.lock);

    return 0;
}

int dns_mark_dump_open(struct inode *inode, struct file *file)
{
    return single_open(file, dns_mark_dump_show, NULL);
}

int dns_mark_debug_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", dns_mark_debug_enabled() ? 1 : 0);
    return 0;
}

int dns_mark_debug_open(struct inode *inode, struct file *file)
{
    return single_open(file, dns_mark_debug_show, NULL);
}

static void dns_mark_free_domain_list(struct list_head *head)
{
    struct dns_mark_domain_entry *entry, *tmp;

    list_for_each_entry_safe(entry, tmp, head, list) {
        list_del(&entry->list);
        kfree(entry->domain_pattern);
        kfree(entry);
    }
}

/* Clear all rules */
ssize_t dns_mark_write_clear(struct file *file, const char __user *buffer,
                             size_t count, loff_t *ppos)
{
    struct dns_mark_rule *rule = NULL, *tmp;
    struct dns_mark_mac_entry *mac_entry, *tmp_mac;
    struct dns_mark_ifname_entry *ifname_entry, *tmp_ifname;

    spin_lock(&rule_mgr.lock);
    list_for_each_entry_safe(rule, tmp, &rule_mgr.rules, list) {
        /* Free ifname entries */
        list_for_each_entry_safe(ifname_entry, tmp_ifname, &rule->ifname_list, list) {
            list_del(&ifname_entry->list);
            kfree(ifname_entry);
        }
        /* Free MAC entries */
        list_for_each_entry_safe(mac_entry, tmp_mac, &rule->mac_list, list) {
            list_del(&mac_entry->list);
            kfree(mac_entry);
        }
        /* Free domain entries */
        dns_mark_free_domain_list(&rule->domain_list);
        /* Remove proc entries */
        if (rule->proc) {
            dns_mark_remove_rule_proc(rule->proc);
            rule->proc = NULL;
        }
        list_del(&rule->list);
        kfree(rule);
    }
    atomic_set(&rule_mgr.rule_count, 0);
    spin_unlock(&rule_mgr.lock);

    /* No rules remain, therefore no domain rules present. */
    atomic_set(&rule_mgr.domain_rules_present, 0);

    return count;
}

int dns_mark_intercept_forward_show(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", dns_mark_intercept_forward_enabled() ? 1 : 0);
    return 0;
}

int dns_mark_intercept_forward_open(struct inode *inode, struct file *file)
{
    return single_open(file, dns_mark_intercept_forward_show, NULL);
}

ssize_t dns_mark_write_intercept_forward(struct file *file, const char __user *buffer,
                             size_t count, loff_t *ppos)
{
    char tmp[8];
    int flag;

    if (count > sizeof(tmp) - 1)
        return -EINVAL;

    if (copy_from_user(tmp, buffer, count))
        return -EFAULT;

    tmp[count] = '\0';
    /* Trim leading and trailing whitespaces/newlines */
    {
        size_t start = 0, end = count;
        while (end > start && isspace((unsigned char)tmp[end - 1]))
            end--;
        while (start < end && isspace((unsigned char)tmp[start]))
            start++;
        if (end == start) {
            /* Only whitespace (e.g., an extra newline write) -> ignore */
            return count;
        }
        tmp[end] = '\0';

        if (kstrtoint(tmp + start, 10, &flag))
            return -EINVAL;
    }

#if defined(CONFIG_JUMP_LABEL)
    if (flag)
        static_branch_enable(&dns_mark_intercept_forward_key);
    else
        static_branch_disable(&dns_mark_intercept_forward_key);
#else
    atomic_set(&dns_mark_intercept_forward_flag, flag ? 1 : 0);
#endif

    return count;
}

ssize_t dns_mark_write_mac_flag(struct file *file, const char __user *buffer,
                                size_t count, loff_t *ppos)
{
    struct dns_mark_rule_proc *rule_proc = PDE_DATA(file_inode(file));
    struct dns_mark_rule *rule = rule_proc->rule;
    char tmp[8];
    int flag;

    if (count > sizeof(tmp) - 1)
        return -EINVAL;

    if (copy_from_user(tmp, buffer, count))
        return -EFAULT;

    tmp[count] = '\0';
    if (kstrtoint(tmp, 10, &flag))
        return -EINVAL;

    if (flag)
        rule->flags |= RULE_MAC_BLACKLIST;
    else
        rule->flags &= ~RULE_MAC_BLACKLIST;

    return count;
}

ssize_t dns_mark_write_domain_flag(struct file *file, const char __user *buffer,
                                   size_t count, loff_t *ppos)
{
    struct dns_mark_rule_proc *rule_proc = PDE_DATA(file_inode(file));
    struct dns_mark_rule *rule = rule_proc->rule;
    char tmp[8];
    int flag;

    if (count > sizeof(tmp) - 1)
        return -EINVAL;

    if (copy_from_user(tmp, buffer, count))
        return -EFAULT;

    tmp[count] = '\0';
    if (kstrtoint(tmp, 10, &flag))
        return -EINVAL;

    if (flag)
        rule->flags |= RULE_DOMAIN_BLACKLIST;
    else
        rule->flags &= ~RULE_DOMAIN_BLACKLIST;

    return count;
}

ssize_t dns_mark_write_mark(struct file *file, const char __user *buffer,
                            size_t count, loff_t *ppos)
{
    struct dns_mark_rule_proc *rule_proc = PDE_DATA(file_inode(file));
    struct dns_mark_rule *rule = rule_proc->rule;
    char tmp[16];
    unsigned int mark;

    if (count > sizeof(tmp) - 1)
        return -EINVAL;

    if (copy_from_user(tmp, buffer, count))
        return -EFAULT;

    tmp[count] = '\0';
    if (kstrtouint(tmp, 16, &mark))
        return -EINVAL;

    rule->mark = mark;
    return count;
}

ssize_t dns_mark_write_debug(struct file *file, const char __user *buffer,
                             size_t count, loff_t *ppos)
{
    char tmp[8];
    int flag;

    if (count > sizeof(tmp) - 1)
        return -EINVAL;

    if (copy_from_user(tmp, buffer, count))
        return -EFAULT;

    tmp[count] = '\0';
    /* Trim leading and trailing whitespaces/newlines */
    {
        size_t start = 0, end = count;
        while (end > start && isspace((unsigned char)tmp[end - 1]))
            end--;
        while (start < end && isspace((unsigned char)tmp[start]))
            start++;
        if (end == start) {
            /* Only whitespace (e.g., an extra newline write) -> ignore */
            return count;
        }
        tmp[end] = '\0';

        if (kstrtoint(tmp + start, 10, &flag))
            return -EINVAL;
    }

#if defined(CONFIG_JUMP_LABEL)
    if (flag)
        static_branch_enable(&dns_mark_debug_key);
    else
        static_branch_disable(&dns_mark_debug_key);
#else
    atomic_set(&dns_mark_debug_flag, flag ? 1 : 0);
#endif

    return count;
}

ssize_t dns_mark_write_macs(struct file *file, const char __user *buffer,
                            size_t count, loff_t *ppos)
{
    struct dns_mark_rule_proc *rule_proc = PDE_DATA(file_inode(file));
    struct dns_mark_rule *rule = rule_proc->rule;
    char *buf;
    char *pos, *end;
    struct dns_mark_mac_entry *entry;
    int ret = count;
    unsigned char mac[ETH_ALEN];

    buf = kmalloc(count + 1, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, buffer, count)) {
        kfree(buf);
        return -EFAULT;
    }
    buf[count] = '\0';

    /* Clear existing MAC entries */
    while (!list_empty(&rule->mac_list)) {
        entry = list_first_entry(&rule->mac_list, struct dns_mark_mac_entry, list);
        list_del(&entry->list);
        kfree(entry);
    }
    atomic_set(&rule->mac_count, 0);

    /* Parse and add new MACs */
    pos = buf;
    while (pos < buf + count) {
        if (atomic_read(&rule->mac_count) >= DNS_MAX_MACS_PER_RULE)
            break;
        end = strchr(pos, '\n');
        if (end)
            *end = '\0';

        if (strlen(pos) > 0 && mac_pton(pos, mac) == 1) {
            entry = kmalloc(sizeof(*entry), GFP_KERNEL);
            if (!entry) {
                ret = -ENOMEM;
                goto out;
            }
            memcpy(entry->mac, mac, ETH_ALEN);
            list_add_tail(&entry->list, &rule->mac_list);
            atomic_inc(&rule->mac_count);
        }

        if (!end)
            break;
        pos = end + 1;
    }

out:
    kfree(buf);
    return ret;
}

ssize_t dns_mark_write_ifnames(struct file *file, const char __user *buffer,
                               size_t count, loff_t *ppos)
{
    struct dns_mark_rule_proc *rule_proc = PDE_DATA(file_inode(file));
    struct dns_mark_rule *rule = rule_proc->rule;
    char *buf;
    char *pos, *end;
    struct dns_mark_ifname_entry *entry;
    int ret = count;

    buf = kmalloc(count + 1, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, buffer, count)) {
        kfree(buf);
        return -EFAULT;
    }
    buf[count] = '\0';

    /* Clear existing ifname entries */
    while (!list_empty(&rule->ifname_list)) {
        entry = list_first_entry(&rule->ifname_list, struct dns_mark_ifname_entry, list);
        list_del(&entry->list);
        kfree(entry);
    }
    atomic_set(&rule->ifname_count, 0);

    /* Parse and add new ifnames */
    pos = buf;
    while (pos < buf + count) {
        end = strchr(pos, '\n');
        if (end)
            *end = '\0';

        if (strlen(pos) > 0) {
            entry = kmalloc(sizeof(*entry), GFP_KERNEL);
            if (!entry) {
                ret = -ENOMEM;
                goto out;
            }
            memcpy(entry->ifname, pos, strlen(pos) + 1);
            list_add_tail(&entry->list, &rule->ifname_list);
            atomic_inc(&rule->ifname_count);
        }

        if (!end)
            break;
        pos = end + 1;
    }

out:
    kfree(buf);
    return ret;
}

ssize_t dns_mark_write_domains(struct file *file, const char __user *buffer,
                               size_t count, loff_t *ppos)
{
    struct dns_mark_rule_proc *rule_proc = PDE_DATA(file_inode(file));
    struct dns_mark_rule *rule = rule_proc->rule;
    char *buf = NULL, *combined_buf = NULL;
    char *pos, *end;
    struct dns_mark_domain_entry *entry;
    int ret = count;
    size_t total_len;

    /* Clear the list and pending data when starting a new write */
    if (*ppos == 0) {
        dns_mark_free_domain_list(&rule->domain_list);
        atomic_set(&rule->domain_count, 0);
        if (dns_mark_write_domains_pending_buf) {
            kfree(dns_mark_write_domains_pending_buf);
            dns_mark_write_domains_pending_buf = NULL;
        }
        dns_mark_write_domains_pending_len = 0;
    }

    /* Allocate buffer for this chunk */
    buf = kmalloc(count + 1, GFP_KERNEL);
    if (!buf)
        return -ENOMEM;

    if (copy_from_user(buf, buffer, count)) {
        ret = -EFAULT;
        goto out;
    }
    buf[count] = '\0';

    /* If we have pending data, combine it with the new data */
    if (dns_mark_write_domains_pending_len > 0) {
        total_len = dns_mark_write_domains_pending_len + count;
        combined_buf = kmalloc(total_len + 1, GFP_KERNEL);
        if (!combined_buf) {
            ret = -ENOMEM;
            goto out;
        }
        memcpy(combined_buf, dns_mark_write_domains_pending_buf, dns_mark_write_domains_pending_len);
        memcpy(combined_buf + dns_mark_write_domains_pending_len, buf, count);
        combined_buf[total_len] = '\0';
        kfree(dns_mark_write_domains_pending_buf);
        dns_mark_write_domains_pending_buf = NULL;
        dns_mark_write_domains_pending_len = 0;
        kfree(buf);
        buf = combined_buf;
        combined_buf = NULL;  /* Prevent double free in error path */
        count = total_len;
    }

    /* Parse and add new domains */
    pos = buf;
    while (pos < buf + count) {
        if (atomic_read(&rule->domain_count) >= DNS_MAX_DOMAINS_PER_RULE)
            break;

        /* Skip leading whitespace */
        while (pos < buf + count && (*pos == ' ' || *pos == '\t' || *pos == '\r'))
            pos++;

        /* Find the end of the current line */
        end = strchr(pos, '\n');
        if (!end) {
            /* No newline found, this might be an incomplete line */
            size_t remaining = buf + count - pos;
            if (remaining > 0) {
                /* Save the incomplete line for the next write */
                dns_mark_write_domains_pending_buf = kmalloc(remaining + 1, GFP_KERNEL);
                if (!dns_mark_write_domains_pending_buf) {
                    ret = -ENOMEM;
                    goto out;
                }
                memcpy(dns_mark_write_domains_pending_buf, pos, remaining);
                dns_mark_write_domains_pending_buf[remaining] = '\0';
                dns_mark_write_domains_pending_len = remaining;
            }
            break;
        }

        *end = '\0';

        if (strlen(pos) > 0 && strlen(pos) < DNS_MAX_DOMAIN_LENGTH) {
            entry = kmalloc(sizeof(*entry), GFP_KERNEL);
            if (!entry) {
                ret = -ENOMEM;
                goto out;
            }

            entry->domain_pattern = kstrdup(pos, GFP_KERNEL);
            if (!entry->domain_pattern) {
                kfree(entry);
                ret = -ENOMEM;
                goto out;
            }

            list_add_tail(&entry->list, &rule->domain_list);
            atomic_inc(&rule->domain_count);
        }

        pos = end + 1;
    }

out:
    if (ret < 0) {
        /* On error, clean up any partially added domains */
        dns_mark_free_domain_list(&rule->domain_list);
        atomic_set(&rule->domain_count, 0);
        if (dns_mark_write_domains_pending_buf) {
            kfree(dns_mark_write_domains_pending_buf);
            dns_mark_write_domains_pending_buf = NULL;
            dns_mark_write_domains_pending_len = 0;
        }
    }
    /* Update cached flag after modifying domain lists */
    dns_mark_recalc_domain_rules_present();
    if (buf && buf != combined_buf)
        kfree(buf);
    if (combined_buf)
        kfree(combined_buf);
    *ppos += (ret < 0 ? 0 : count);
    return ret;
}

ssize_t dns_mark_write_create(struct file *file, const char __user *buffer,
                              size_t count, loff_t *ppos)
{
    char rule_id_str[32];
    int rule_id;
    struct dns_mark_rule *rule;
    int ret;
    char dirname[32];
    struct proc_dir_entry *test_entry;

    if (count >= sizeof(rule_id_str))
        return -EINVAL;

    if (copy_from_user(rule_id_str, buffer, count))
        return -EFAULT;

    rule_id_str[count] = '\0';
    if (kstrtoint(rule_id_str, 10, &rule_id) < 0)
        return -EINVAL;

    /* Check if rule directory already exists by trying to create a test entry */
    snprintf(dirname, sizeof(dirname), "rule%d", rule_id);
    test_entry = proc_mkdir(dirname, proc_dir.proc_root);
    if (!test_entry) {
        pr_info("Rule directory %s already exists\n", dirname);
        return -EEXIST;
    }
    proc_remove(test_entry);

    /* Create a new rule */
    rule = kzalloc(sizeof(*rule), GFP_KERNEL);
    if (!rule)
        return -ENOMEM;

    INIT_LIST_HEAD(&rule->ifname_list);
    INIT_LIST_HEAD(&rule->mac_list);
    INIT_LIST_HEAD(&rule->domain_list);
    atomic_set(&rule->ifname_count, 0);
    atomic_set(&rule->mac_count, 0);
    atomic_set(&rule->domain_count, 0);
    rule->rule_id = rule_id;  /* Set the rule ID */

    /* Create proc entries for this rule */
    ret = dns_mark_create_rule_proc(rule, rule_id);
    if (ret < 0) {
        kfree(rule);
        return ret;
    }

    /* Add rule to list */
    spin_lock(&rule_mgr.lock);
    list_add_tail(&rule->list, &rule_mgr.rules);
    atomic_inc(&rule_mgr.rule_count);
    spin_unlock(&rule_mgr.lock);

    return count;
}

int dns_mark_ifnames_show(struct seq_file *m, void *v)
{
    struct dns_mark_rule_proc *rule_proc = m->private;
    struct dns_mark_rule *rule = rule_proc->rule;
    struct dns_mark_ifname_entry *ifname_entry;

    list_for_each_entry(ifname_entry, &rule->ifname_list, list) {
        seq_printf(m, "%s\n", ifname_entry->ifname);
    }
    return 0;
}

int dns_mark_ifnames_open(struct inode *inode, struct file *file)
{
    return single_open(file, dns_mark_ifnames_show, PDE_DATA(inode));
}

int dns_mark_macs_show(struct seq_file *m, void *v)
{
    struct dns_mark_rule_proc *rule_proc = m->private;
    struct dns_mark_rule *rule = rule_proc->rule;
    struct dns_mark_mac_entry *mac_entry;

    list_for_each_entry(mac_entry, &rule->mac_list, list) {
        seq_printf(m, "%pM\n", mac_entry->mac);
    }
    return 0;
}

int dns_mark_macs_open(struct inode *inode, struct file *file)
{
    return single_open(file, dns_mark_macs_show, PDE_DATA(inode));
}

int dns_mark_domains_show(struct seq_file *m, void *v)
{
    struct dns_mark_rule_proc *rule_proc = m->private;
    struct dns_mark_rule *rule = rule_proc->rule;
    struct dns_mark_domain_entry *entry = NULL;

    list_for_each_entry(entry, &rule->domain_list, list) {
        if (entry->domain_pattern[0] != '\0') {
            seq_printf(m, "%s\n", entry->domain_pattern);
        }
    }

    return 0;
}

int dns_mark_domains_open(struct inode *inode, struct file *file)
{
    return single_open(file, dns_mark_domains_show, PDE_DATA(inode));
}

int dns_mark_mark_show(struct seq_file *m, void *v)
{
    struct dns_mark_rule_proc *rule_proc = m->private;
    struct dns_mark_rule *rule = rule_proc->rule;
    seq_printf(m, "0x%x\n", rule->mark);
    return 0;
}

int dns_mark_mark_open(struct inode *inode, struct file *file)
{
    return single_open(file, dns_mark_mark_show, PDE_DATA(inode));
}

int dns_mark_mac_flag_show(struct seq_file *m, void *v)
{
    struct dns_mark_rule_proc *rule_proc = m->private;
    struct dns_mark_rule *rule = rule_proc->rule;
    seq_printf(m, "%d\n", !!(rule->flags & RULE_MAC_BLACKLIST));
    return 0;
}

int dns_mark_mac_flag_open(struct inode *inode, struct file *file)
{
    return single_open(file, dns_mark_mac_flag_show, PDE_DATA(inode));
}

int dns_mark_domain_flag_show(struct seq_file *m, void *v)
{
    struct dns_mark_rule_proc *rule_proc = m->private;
    struct dns_mark_rule *rule = rule_proc->rule;
    seq_printf(m, "%d\n", !!(rule->flags & RULE_DOMAIN_BLACKLIST));
    return 0;
}

int dns_mark_domain_flag_open(struct inode *inode, struct file *file)
{
    return single_open(file, dns_mark_domain_flag_show, PDE_DATA(inode));
}

/* Create proc entries for a rule */
int dns_mark_create_rule_proc(struct dns_mark_rule *rule, int rule_id)
{
    struct dns_mark_rule_proc *rule_proc;
    char dirname[32];

    rule_proc = kzalloc(sizeof(*rule_proc), GFP_KERNEL);
    if (!rule_proc)
        return -ENOMEM;

    rule_proc->rule = rule;

    /* Create rule directory */
    snprintf(dirname, sizeof(dirname), "rule%d", rule_id);
    rule_proc->rule_dir = proc_mkdir(dirname, proc_dir.proc_root);
    if (!rule_proc->rule_dir)
        goto err_free;

    /* Create dump file for this rule */
    rule_proc->dump = proc_create_data("dump", 0444, rule_proc->rule_dir,
                                       &dns_mark_rule_dump_ops, rule_proc);
    if (!rule_proc->dump)
        goto err_remove;

    /* Create ifnames file for this rule */
    rule_proc->ifnames = proc_create_data("ifnames", 0644, rule_proc->rule_dir,
                                          &dns_mark_ifnames_ops, rule_proc);
    if (!rule_proc->ifnames)
        goto err_remove;

    /* Create MAC file for this rule */
    rule_proc->macs = proc_create_data("macs", 0644, rule_proc->rule_dir,
                                       &dns_mark_macs_ops, rule_proc);
    if (!rule_proc->macs)
        goto err_remove;

    /* Create domains file for this rule */
    rule_proc->domains = proc_create_data("domains", 0644, rule_proc->rule_dir,
                                          &dns_mark_domains_ops, rule_proc);
    if (!rule_proc->domains)
        goto err_remove;

    /* Create mark file for this rule */
    rule_proc->mark = proc_create_data("mark", 0644, rule_proc->rule_dir,
                                       &dns_mark_mark_ops, rule_proc);
    if (!rule_proc->mark)
        goto err_remove;

    /* Create mac_flag file for this rule */
    rule_proc->mac_flag = proc_create_data("mac_flag", 0644, rule_proc->rule_dir,
                                           &dns_mark_mac_flag_ops, rule_proc);
    if (!rule_proc->mac_flag)
        goto err_remove;

    /* Create domain_flag file for this rule */
    rule_proc->domain_flag = proc_create_data("domain_flag", 0644, rule_proc->rule_dir,
                             &dns_mark_domain_flag_ops, rule_proc);
    if (!rule_proc->domain_flag)
        goto err_remove;

    rule->proc = rule_proc;
    return 0;

err_remove:
    dns_mark_remove_rule_proc(rule_proc);
    return -ENOMEM;

err_free:
    kfree(rule_proc);
    return -ENOMEM;
}

/* Remove proc entries for a rule */
void dns_mark_remove_rule_proc(struct dns_mark_rule_proc *rule_proc)
{
    if (!rule_proc)
        return;

    if (rule_proc->mark)
        remove_proc_entry("mark", rule_proc->rule_dir);
    if (rule_proc->domain_flag)
        remove_proc_entry("domain_flag", rule_proc->rule_dir);
    if (rule_proc->mac_flag)
        remove_proc_entry("mac_flag", rule_proc->rule_dir);
    if (rule_proc->domains)
        remove_proc_entry("domains", rule_proc->rule_dir);
    if (rule_proc->ifnames)
        remove_proc_entry("ifnames", rule_proc->rule_dir);
    if (rule_proc->macs)
        remove_proc_entry("macs", rule_proc->rule_dir);
    if (rule_proc->dump)
        remove_proc_entry("dump", rule_proc->rule_dir);

    if (rule_proc->rule_dir) {
        char dirname[32];
        snprintf(dirname, sizeof(dirname), "rule%d", rule_proc->rule->rule_id);
        remove_proc_entry(dirname, proc_dir.proc_root);
    }

    kfree(rule_proc);
}

/* Initialize proc filesystem entries */
int dns_mark_proc_init(void)
{
    proc_dir.proc_root = proc_mkdir("dns_mark", NULL);
    if (!proc_dir.proc_root)
        return -ENOMEM;

    /* Create dump and clear entries */
    proc_dir.dump = proc_create_data("dump", 0444, proc_dir.proc_root,
                                     &dns_mark_dump_ops, NULL);
    proc_dir.clear = proc_create_data("clear", 0200, proc_dir.proc_root,
                                      &dns_mark_clear_ops, NULL);
    proc_dir.create = proc_create_data("create", 0200, proc_dir.proc_root,
                                       &dns_mark_create_ops, NULL);
    proc_dir.debug = proc_create_data("debug", 0644, proc_dir.proc_root,
                                      &dns_mark_debug_ops, NULL);
    proc_dir.intercept_forward = proc_create_data("intercept_forward", 0644, proc_dir.proc_root,
                                      &dns_mark_intercept_forward_ops, NULL);
    if (!proc_dir.dump || !proc_dir.clear || !proc_dir.create || !proc_dir.debug || !proc_dir.intercept_forward) {
        dns_mark_proc_cleanup();
        return -ENOMEM;
    }

    return 0;
}

/* Cleanup proc filesystem entries */
void dns_mark_proc_cleanup(void)
{
    struct dns_mark_rule *rule = NULL, *tmp;

    /* Remove all rules */
    spin_lock(&rule_mgr.lock);
    list_for_each_entry_safe(rule, tmp, &rule_mgr.rules, list) {
        list_del(&rule->list);
        if (rule->proc)
            dns_mark_remove_rule_proc(rule->proc);
        /* Free ifname entries */
        while (!list_empty(&rule->ifname_list)) {
            struct dns_mark_ifname_entry *entry = list_first_entry(&rule->ifname_list,
                                                  struct dns_mark_ifname_entry,
                                                  list);
            list_del(&entry->list);
            kfree(entry);
        }
        /* Free MAC entries */
        while (!list_empty(&rule->mac_list)) {
            struct dns_mark_mac_entry *entry = list_first_entry(&rule->mac_list,
                                               struct dns_mark_mac_entry,
                                               list);
            list_del(&entry->list);
            kfree(entry);
        }
        /* Free domain entries */
        while (!list_empty(&rule->domain_list)) {
            struct dns_mark_domain_entry *entry = list_first_entry(&rule->domain_list,
                                                  struct dns_mark_domain_entry,
                                                  list);
            list_del(&entry->list);
            kfree(entry->domain_pattern);
            kfree(entry);
        }
        kfree(rule);
    }
    spin_unlock(&rule_mgr.lock);

    /* Remove proc entries */
    if (proc_dir.create)
        remove_proc_entry("create", proc_dir.proc_root);
    if (proc_dir.debug)
        remove_proc_entry("debug", proc_dir.proc_root);
    if (proc_dir.intercept_forward)
        remove_proc_entry("intercept_forward", proc_dir.proc_root);
    if (proc_dir.clear)
        remove_proc_entry("clear", proc_dir.proc_root);
    if (proc_dir.dump)
        remove_proc_entry("dump", proc_dir.proc_root);
    if (proc_dir.proc_root)
        remove_proc_entry("dns_mark", NULL);

    /* Reset cached flag */
    atomic_set(&rule_mgr.domain_rules_present, 0);
}
