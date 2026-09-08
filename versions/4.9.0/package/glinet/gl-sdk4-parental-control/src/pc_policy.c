#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/inet.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include "pc_policy.h"
#include "cJSON.h"

struct list_head pc_rule_head = LIST_HEAD_INIT(pc_rule_head);
struct list_head pc_group_head = LIST_HEAD_INIT(pc_group_head);

DEFINE_RWLOCK(pc_policy_lock);
static void rule_init_list(pc_rule_t *rule)
{
    rule->blist.next = &rule->blist;
    rule->blist.prev = &rule->blist;
    rule->applist.next = &rule->applist;
    rule->applist.prev = &rule->applist;
    rule->domain_list.next = &rule->domain_list;
    rule->domain_list.prev = &rule->domain_list;
}

static void rule_clean_list(pc_rule_t *rule)
{
    pc_app_t *app;
    pc_app_index_t *index;
    pc_domain_t *domain;

    while (!list_empty(&rule->blist)) {
        app = list_first_entry(&rule->blist, pc_app_t, head);
        list_del(&(app->head));
        kfree(app);
    }

    while (!list_empty(&rule->applist)) {
        index = list_first_entry(&rule->applist, pc_app_index_t, head);
        list_del(&(index->head));
        kfree(index);
    }

    while (!list_empty(&rule->domain_list)) {
        domain = list_first_entry(&rule->domain_list, pc_domain_t, head);
        list_del(&(domain->head));
        kfree(domain);
    }
}

static int rule_add_blist_item(pc_rule_t *rule, const char *str)
{
    pc_app_t *node = NULL;
    pc_domain_t *domain_node = NULL;
    const char *p = NULL;
    u_int32_t len, size = 0;
    if (!str) {
        return -1;
    }

    /*[tcp;;;baidu.com;;]*/
    if (!strncmp(str, "tcp;;;", strlen("tcp;;;"))) {
        len = strlen(str);
        if (len > strlen("tcp;;;") && strcmp(str + len - 2, ";;") == 0) {
            domain_node = kzalloc(sizeof(pc_domain_t), GFP_KERNEL);
            if (domain_node == NULL) {
                return -1;
            }
            p = str + strlen("tcp;;;");
            size = strlen(p) - 1;
            if (size >= sizeof(domain_node->domain)) {
                size = sizeof(domain_node->domain);
            }
            snprintf(domain_node->domain, size, "%s", p);
            list_add(&(domain_node->head), &rule->domain_list);
            return 0;
        }
    }

    node = kzalloc(sizeof(pc_app_t), GFP_KERNEL);

    if (node == NULL) {
        printk("malloc feature memory error\n");
        return -1;
    } else {
        if (!pc_set_app_by_str(node, BLIST_ID, "blacklist", str)) {
            list_add(&(node->head), &rule->blist);
        } else {
            kfree(node);
            return -1;
        }
    }

    return 0;
}

static void rule_add_blist(pc_rule_t *rule, cJSON *list)
{
    int size, j;
    cJSON *item = NULL;

    if (list) {
        size = cJSON_GetArraySize(list);
        for (j = 0; j < size; j++) {
            item = cJSON_GetArrayItem(list, j);
            if (item) {
                rule_add_blist_item(rule, item->valuestring);
            }
        }
    }
}

static int rule_add_applist_item(pc_rule_t *rule, u_int32_t id)
{
    pc_app_index_t *node = NULL;

    node = kzalloc(sizeof(pc_app_index_t), GFP_KERNEL);
    if (node == NULL) {
        printk("malloc feature memory error\n");
        return -1;
    } else {
        node->app_id = id;
        list_add(&(node->head), &rule->applist);
    }

    return 0;
}

static void rule_add_applist(pc_rule_t *rule, cJSON *list)
{
    int size, j;
    cJSON *item = NULL;

    if (list) {
        size = cJSON_GetArraySize(list);
        for (j = 0; j < size; j++) {
            item = cJSON_GetArrayItem(list, j);
            if (item) {
                rule_add_applist_item(rule, item->valueint);
            }
        }
    }
}

int add_pc_rule(const char *id,  cJSON *applist, enum pc_action action,
                cJSON *blist)
{
    pc_rule_t *rule = NULL;

    rule = kzalloc(sizeof(pc_rule_t), GFP_KERNEL);
    if (rule == NULL) {
        printk("malloc pc_rule_t memory error\n");
        return -1;
    } else {
        memcpy(rule->id, id, RULE_ID_SIZE);
        rule->action = action;
        rule->refer_count = 0;
        rule_init_list(rule);
        rule_add_blist(rule, blist);
        rule_add_applist(rule, applist);
        pc_policy_write_lock();
        list_add_rcu(&rule->head, &pc_rule_head);
        pc_policy_write_unlock();
    }

    return 0;
}

static void rcu_free_rule(struct rcu_head *head)
{
    pc_rule_t *rule = container_of(head, pc_rule_t, rcu);
    kfree(rule);
}

static int __maybe_unused remove_pc_rule(const char *id)
{
    pc_rule_t *rule = NULL, *n;

    list_for_each_entry_safe(rule, n, &pc_rule_head, head) {
        if (strcmp(rule->id, id) == 0) {
            if (rule->refer_count > 0) {
                printk("refer_count of rule != 0\n");
                return -1;
            }
            pc_policy_write_lock();
            list_del(&rule->head);
            rule_clean_list(rule);
            call_rcu(&rule->rcu, rcu_free_rule);
            pc_policy_write_unlock();
        }
    }

    return 0;
}

int clean_pc_rule(void)
{
    pc_rule_t *rule = NULL, *n;

    pc_policy_write_lock();
    list_for_each_entry_safe(rule, n, &pc_rule_head, head) {
        list_del(&rule->head);
        rule_clean_list(rule);
        call_rcu(&rule->rcu, rcu_free_rule);
    }
    pc_policy_write_unlock();
    rcu_barrier();

    return 0;
}

int set_pc_rule(const char *id, cJSON *applist, enum pc_action action,
                cJSON *blist)
{
    pc_rule_t *rule = NULL, *n;

    pc_policy_write_lock();
    list_for_each_entry_safe(rule, n, &pc_rule_head, head) {
        if (strcmp(rule->id, id) == 0) {
            memcpy(rule->id, id, RULE_ID_SIZE);
            rule_clean_list(rule);
            rule_init_list(rule);
            rule_add_blist(rule, blist);
            rule_add_applist(rule, applist);
            rule->action = action;
        }
    }
    pc_policy_write_unlock();

    return 0;
}

static pc_rule_t *find_rule_by_id(const char *id)
{
    pc_rule_t *rule = NULL;
    pc_rule_t *ret = NULL;

    rcu_read_lock_bh();
    list_for_each_entry_rcu(rule, &pc_rule_head, head) {
        if (strcmp(rule->id, id) == 0) {
            ret = rule;
            goto out;
        }
    }

out:
    rcu_read_unlock_bh();
    return ret;
}

static void group_init_list(pc_group_t *group)
{
    group->macs.next = &group->macs;
    group->macs.prev = &group->macs;
}

static void group_clean_list(pc_group_t *group)
{
    pc_mac_t *mac, *n;

    list_for_each_entry_safe(mac, n, &group->macs, head) {
        list_del(&(mac->head));
        kfree(mac);
    }
}

static int mac_to_hex(const char *mac, u8 *mac_hex)
{
    u32 mac_tmp[ETH_ALEN];
    int ret = 0, i = 0;

    ret = sscanf(mac, "%02x:%02x:%02x:%02x:%02x:%02x",
                 (unsigned int *)&mac_tmp[0],
                 (unsigned int *)&mac_tmp[1],
                 (unsigned int *)&mac_tmp[2],
                 (unsigned int *)&mac_tmp[3],
                 (unsigned int *)&mac_tmp[4],
                 (unsigned int *)&mac_tmp[5]);

    if (ETH_ALEN != ret)
        return -1;

    for (i = 0; i < ETH_ALEN; i++)
        mac_hex[i] = mac_tmp[i];

    return 0;
}

static int group_add_mac_item(pc_group_t *group, const char *str)
{
    pc_mac_t *node = NULL;

    node = kzalloc(sizeof(pc_mac_t), GFP_ATOMIC);
    if (node == NULL) {
        printk("malloc mac node memory error\n");
        return -1;
    } else {
        if (!mac_to_hex(str, node->mac)) {
            list_add(&(node->head), &group->macs);
        } else {
            kfree(node);
            return -1;
        }
    }

    return 0;
}

static void group_add_macs(pc_group_t *group, cJSON *list)
{
    int size, j;
    cJSON *item = NULL;

    if (list) {
        size = cJSON_GetArraySize(list);
        for (j = 0; j < size; j++) {
            item = cJSON_GetArrayItem(list, j);
            if (item)
                group_add_mac_item(group, item->valuestring);
        }
    }
}

int add_pc_group(const char *id,  cJSON *macs, const char *rule_id)
{
    pc_group_t *group = NULL;
    pc_rule_t *rule = NULL;

    group = kzalloc(sizeof(pc_group_t), GFP_ATOMIC);
    if (group == NULL) {
        printk("malloc pc_group_t memory error\n");
        return -1;
    } else {
        memcpy(group->id, id, GROUP_ID_SIZE);
        group_init_list(group);
        group_add_macs(group, macs);
        rule = find_rule_by_id(rule_id);
        group->rule = rule;
        pc_policy_write_lock();
        if (rule) {
            rule->refer_count += 1;//增加规则引用计数
        }
        list_add_rcu(&group->head, &pc_group_head);
        pc_policy_write_unlock();
    }

    return 0;
}

static void rcu_free_group(struct rcu_head *head)
{
    pc_group_t *group = container_of(head, pc_group_t, rcu);
    kfree(group);
}

static int __maybe_unused remove_pc_group(const char *id)
{
    pc_group_t *group = NULL, *n;
    pc_rule_t *rule = NULL;

    pc_policy_write_lock();
    list_for_each_entry_safe(group, n, &pc_group_head, head) {
        if (strcmp(group->id, id) == 0) {
            rule = group->rule;
            if (rule)
                rule->refer_count -= 1;
            group_clean_list(group);
            list_del_rcu(&group->head);
            call_rcu(&group->rcu, rcu_free_group);

        }
    }
    pc_policy_write_unlock();

    return 0;
}

int clean_pc_group(void)
{
    pc_group_t *group = NULL, *n;
    pc_rule_t *rule = NULL;

    pc_policy_write_lock();
    list_for_each_entry_safe(group, n, &pc_group_head, head) {
        rule = group->rule;
        if (rule)
            rule->refer_count -= 1;
        group_clean_list(group);
        list_del_rcu(&group->head);
        call_rcu(&group->rcu, rcu_free_group);
    }
    pc_policy_write_unlock();
    rcu_barrier();

    return 0;
}

int set_pc_group(const char *id,  cJSON *macs, const char *rule_id)
{
    pc_group_t *group = NULL, *n;
    pc_rule_t *rule = NULL;

    rule = find_rule_by_id(rule_id);
    PC_DEBUG("set rule %s for group %s\n", rule ? rule->id : "NULL", id);

    pc_policy_write_lock();
    list_for_each_entry_safe(group, n, &pc_group_head, head) {
        if (strcmp(group->id, id) == 0) {
            PC_DEBUG("match group %s\n", group->id);
            group_clean_list(group);
            group_add_macs(group, macs);
            if (group->rule)
                group->rule->refer_count -= 1;//减少旧规则的引用计数
            group->rule = rule;
            if (rule)
                rule->refer_count += 1;//增加被引用规则的引用计数

        }
    }
    pc_policy_write_unlock();

    return 0;
}

static int find_groups_by_mac(u8 mac[ETH_ALEN], pc_group_t *groups, int max_groups)
{
    pc_group_t *group = NULL;
    pc_mac_t *nmac = NULL;
    int count = 0;

    list_for_each_entry_rcu(group, &pc_group_head, head) {
        if (count >= max_groups)
            break;
        list_for_each_entry(nmac, &group->macs, head) {
            if (ether_addr_equal(nmac->mac, mac)) {
                groups[count++] = *group;
                break;
            }
        }
    }

    return count;
}

int get_rule_by_mac(u8 mac[ETH_ALEN], pc_rule_t **rules, int max_rules, enum pc_action *action)
{
    pc_group_t groups[MAX_GROUPS_PER_MAC];
    int i, rule_count = 0;
    bool has_drop = false, has_policy_drop = false;

    if (!rules || !action || max_rules <= 0)
        return 0;

    rcu_read_lock();
    rule_count = find_groups_by_mac(mac, groups, max_rules);
    if (rule_count == 0) {
        rcu_read_unlock();
        if (pc_drop_anonymous) {
            *action = PC_DROP_ANONYMOUS;
        } else
            *action = PC_ACCEPT;
        return 0;
    }

    for (i = 0; i < rule_count && i < max_rules; i++) {
        if (groups[i].rule) {
            rules[i] = groups[i].rule;
            if (groups[i].rule->action == PC_DROP)
                has_drop = true;
            else if (groups[i].rule->action == PC_POLICY_DROP)
                has_policy_drop = true;
        } else {
            rules[i] = NULL;
        }
    }
    rcu_read_unlock();

    if (has_drop)
        *action = PC_DROP;
    else if (has_policy_drop)
        *action = PC_POLICY_DROP;
    else
        *action = PC_ACCEPT;

    return rule_count;
}

static int rule_blist_print(struct seq_file *s, pc_rule_t *rule)
{
    range_value_t port_range;
    pc_app_t *app = NULL;
    pc_domain_t *domain = NULL;
    int i;

    seq_printf(s, "Black List:\n");
    seq_printf(s, "ID\tName\tProto\tSport\tDport\tHost_url\tRequest_url\tDataDictionary\n");

    list_for_each_entry(app, &rule->blist, head) {
        seq_printf(s, "%d\t%s\t%d\t%d\t", app->app_id, app->app_name, app->proto, app->sport);
        for (i = 0; i < app->dport_info.num; i++) {
            port_range = app->dport_info.range_list[i];
            (i == 0) ? seq_printf(s, "%s", port_range.not ? "!" : "") :
            seq_printf(s, "%s", port_range.not ? "|!" : "|");
            (port_range.start == port_range.end) ?
            seq_printf(s, "%d", port_range.start) :
            seq_printf(s, "%d-%d", port_range.start, port_range.end);
        }
        if (app->dport_info.num)
            seq_printf(s, "\t");
        seq_printf(s, "%s\t%s", app->host_url, app->request_url);

        for (i = 0; i < app->pos_num; i++)
            seq_printf(s, "%s[%d]=0x%x", (i == 0) ? "\t" : "&&", app->pos_info[i].pos, app->pos_info[i].value);

        seq_printf(s, "\n");
    }

    list_for_each_entry(domain, &rule->domain_list, head) {
        seq_printf(s, "%d\t%s\t%d\t%d\t", -1, "domain_list", 0, 0);
        seq_printf(s, "%s\t", domain->domain);
        seq_printf(s, "\n");
    }

    return 0;
}

static int rule_proc_show(struct seq_file *s, void *v)
{
    pc_rule_t *rule = NULL;
    pc_app_index_t *index = NULL;

    seq_printf(s, "ID\tAction\tRefer_count\tAPPs\n");
    pc_policy_read_lock();
    list_for_each_entry(rule, &pc_rule_head, head) {
        seq_printf(s, "%s\t%d\t%d\t[ ", rule->id, rule->action, rule->refer_count);
        list_for_each_entry(index, &rule->applist, head) {
            seq_printf(s, "%d ", index->app_id);
        }
        seq_printf(s, "]\n");
        rule_blist_print(s, rule);
        seq_printf(s, "=======================================================\n\n");
    }
    pc_policy_read_unlock();

    return 0;
}

static int rule_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, rule_proc_show, NULL);
}

static int group_proc_show(struct seq_file *s, void *v)
{
    pc_group_t *group = NULL;
    pc_mac_t *mac = NULL;

    seq_printf(s, "ID\tRule_ID\tMACs\n");

    pc_policy_read_lock();
    list_for_each_entry(group, &pc_group_head, head) {
        seq_printf(s, "%s\t%s\t[ ", group->id, group->rule ? group->rule->id : "NULL");
        list_for_each_entry(mac, &group->macs, head) {
            seq_printf(s, "%pM ", mac->mac);
        }
        seq_printf(s, "]\n");
    }
    pc_policy_read_unlock();

    return 0;
}

static int group_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, group_proc_show, NULL);
}

static int drop_anonymous_show(struct seq_file *s, void *v)
{
    seq_printf(s, pc_drop_anonymous ? "YES\n" : "NO\n");
    return 0;
}

static int drop_anonymous_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, drop_anonymous_show, NULL);
}

static int app_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, app_proc_show, NULL);
}

static int src_dev_show(struct seq_file *s, void *v)
{
    seq_printf(s, "%s\n", pc_src_dev);
    return 0;
}

static int src_dev_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, src_dev_show, NULL);
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(5, 5, 0)
static const struct file_operations pc_app_fops = {
    .owner = THIS_MODULE,
    .open = app_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
static const struct file_operations pc_rule_fops = {
    .owner = THIS_MODULE,
    .open = rule_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
static const struct file_operations pc_group_fops = {
    .owner = THIS_MODULE,
    .open = group_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
static const struct file_operations pc_drop_anonymous_fops = {
    .owner = THIS_MODULE,
    .open = drop_anonymous_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
static const struct file_operations pc_src_dev_fops = {
    .owner = THIS_MODULE,
    .open = src_dev_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};
#else
static const struct proc_ops pc_app_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = app_proc_open,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
static const struct proc_ops pc_rule_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = rule_proc_open,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
static const struct proc_ops pc_group_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = group_proc_open,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
static const struct proc_ops pc_drop_anonymous_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = drop_anonymous_proc_open,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
static const struct proc_ops pc_src_dev_fops = {
    .proc_flags = PROC_ENTRY_PERMANENT,
    .proc_read = seq_read,
    .proc_open = src_dev_proc_open,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
#endif

static int pc_init_procfs(void)
{
    struct proc_dir_entry *proc;

    proc = proc_mkdir("parental-control", NULL);
    if (!proc) {
        PC_ERROR("can't create dir /proc/parental-control/\n");
        return -ENODEV;;
    }

    proc_create("rule", 0644, proc, &pc_rule_fops);
    proc_create("group", 0644, proc, &pc_group_fops);
    proc_create("app", 0644, proc, &pc_app_fops);
    proc_create("drop_anonymous", 0644, proc, &pc_drop_anonymous_fops);
    proc_create("src_dev", 0644, proc, &pc_src_dev_fops);

    return 0;
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("luochognjun@gl-inet.com");
MODULE_DESCRIPTION("parental control module");
MODULE_VERSION("1.0");

static int __init pc_policy_init(void)
{
    if (pc_load_app_feature_list())
        return -1;

    if (pc_register_dev())
        goto free_app;

    if (pc_filter_init())
        goto free_dev;

    pc_init_procfs();

    return 0;

free_dev:
    pc_unregister_dev();
free_app:
    pc_clean_app_feature_list();
    return -1;
}

static void pc_policy_exit(void)
{
    remove_proc_subtree("parental-control", NULL);
    pc_filter_exit();
    pc_unregister_dev();
    clean_pc_group();
    clean_pc_rule();
    pc_clean_app_feature_list();

    return;
}

module_init(pc_policy_init);
module_exit(pc_policy_exit);
