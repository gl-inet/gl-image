#ifndef _DNS_MARK_H
#define _DNS_MARK_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/jump_label.h>

#define DNS_MAX_DOMAIN_LENGTH 253  /* Maximum length of a domain name */
#define DNS_MAX_LABEL_LENGTH 63   /* Maximum length of a single label */
#define DNS_MAX_MACS_PER_RULE 1024  /* Maximum number of MAC addresses per rule */
#define DNS_MAX_DOMAINS_PER_RULE 500000  /* Maximum number of domains per rule */
#define DNS_MARK_MAX_RULES 1024
#define DNS_MAX_IFNAME_LENGTH 32

/* handle standard port 53 and adguard port 3053 */
#define DNS_PORT_STD 53
#define DNS_PORT_ADG 3053

/* Rule flags */
#define RULE_MAC_BLACKLIST    0x0001  /* MAC addresses in blacklist mode */
#define RULE_DOMAIN_BLACKLIST 0x0002  /* Domains in blacklist mode */

/* Interface list entry */
struct dns_mark_ifname_entry {
    struct list_head list;
    char ifname[DNS_MAX_IFNAME_LENGTH];
};

/* MAC address entry */
struct dns_mark_mac_entry {
    struct list_head list;
    unsigned char mac[ETH_ALEN];
};

/* Domain pattern entry */
struct dns_mark_domain_entry {
    struct list_head list;
    char *domain_pattern;
};

/* Rule structure */
struct dns_mark_rule {
    struct list_head list;
    struct list_head ifname_list; /* List of interface names */
    struct list_head mac_list;    /* List of MAC addresses */
    struct list_head domain_list; /* List of domain patterns */
    u32 mark;
    u32 flags;  /* Rule flags for blacklist modes */
    atomic_t mac_count;    /* Number of MAC addresses */
    atomic_t ifname_count; /* Number of ifname */
    atomic_t domain_count; /* Number of domain patterns */
    int rule_id;          /* Rule ID for proc filesystem */
    struct dns_mark_rule_proc *proc; /* Proc entries for this rule */
};

/* Rule manager */
struct dns_mark_rule_mgr {
    struct list_head rules;
    spinlock_t lock;
    atomic_t rule_count;
    struct dns_mark_proc_dir *proc_dir; /* Proc directory entries */
    atomic_t domain_rules_present; /* 1 if any rule has domain entries, else 0 */
};

/* DNS packet parsing */
struct dns_query_info {
    char domain[DNS_MAX_DOMAIN_LENGTH];
    size_t domain_len;
};

/* Function declarations */
int dns_mark_init_rules(void);
void dns_mark_cleanup_rules(void);
int dns_mark_parse_query(struct sk_buff *skb, struct dns_query_info *info);
int dns_mark_parse_query6(struct sk_buff *skb, struct dns_query_info *info);
u32 dns_mark_apply_rules(struct sk_buff *skb, struct dns_query_info *info);
bool dns_mark_match_domain(const char *pattern, const char *domain);
/* Extern for global rule manager defined in main.c */
extern struct dns_mark_rule_mgr rule_mgr;

static inline bool dns_mark_port_is_dns(u16 port)
{
    return port == DNS_PORT_STD || port == DNS_PORT_ADG;
}

/* Return true if any rule contains at least one domain entry (i.e., domain parsing needed) */
static inline bool dns_mark_need_domain_parse(void)
{
    return atomic_read(&rule_mgr.domain_rules_present) != 0;
}

/* Debug switch using static keys for near-zero overhead when disabled */
#if defined(CONFIG_JUMP_LABEL)
extern struct static_key_false dns_mark_debug_key;
static inline bool dns_mark_debug_enabled(void)
{
    return static_branch_unlikely(&dns_mark_debug_key);
}
#else
/* Fallback for kernels without jump label: use an atomic flag */
extern atomic_t dns_mark_debug_flag;
static inline bool dns_mark_debug_enabled(void)
{
    return atomic_read(&dns_mark_debug_flag) != 0;
}
#endif

/* Intercept-forward global toggle (default ON) */
#if defined(CONFIG_JUMP_LABEL)
extern struct static_key_true dns_mark_intercept_forward_key;
static inline bool dns_mark_intercept_forward_enabled(void)
{
    return static_branch_unlikely(&dns_mark_intercept_forward_key);
}
#else
extern atomic_t dns_mark_intercept_forward_flag;
static inline bool dns_mark_intercept_forward_enabled(void)
{
    return atomic_read(&dns_mark_intercept_forward_flag) != 0;
}
#endif

#define DNS_MARK_DBG_RL(fmt, ...)                       \
    do {                                                \
        if (dns_mark_debug_enabled())                   \
            pr_info_ratelimited(fmt, ##__VA_ARGS__);    \
    } while (0)

#endif /* _DNS_MARK_H */
