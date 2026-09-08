#include <linux/slab.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/version.h>
#include "dns_mark.h"

extern struct dns_mark_rule_mgr rule_mgr;  /* Defined in main.c */

/* Domain pattern matching */
bool dns_mark_match_domain(const char *pattern, const char *domain)
{
    size_t pattern_len, domain_len;

    if (!pattern || !domain)
        return false;

    pattern_len = strlen(pattern);
    domain_len = strlen(domain);

    /* Exact match */
    if (strcasecmp(pattern, domain) == 0)
        return true;

    /* Check if domain is a subdomain of pattern */
    if (domain_len > pattern_len &&
            domain[domain_len - pattern_len - 1] == '.' &&
            strcasecmp(domain + domain_len - pattern_len, pattern) == 0)
        return true;

    return false;
}


/* Check if MAC address matches the rule */
static bool dns_mark_match_mac(struct dns_mark_rule *rule, const unsigned char *mac)
{
    struct dns_mark_mac_entry *entry;
    bool found = false;

    /* If no MAC entries, consider it as matched when blacklist mode*/
    if (atomic_read(&rule->mac_count) == 0)
        return (rule->flags & RULE_MAC_BLACKLIST) ? !found : found;

    list_for_each_entry(entry, &rule->mac_list, list) {
        if (ether_addr_equal(entry->mac, mac)) {
            found = true;
            break;
        }
    }

    /* If blacklist mode, reverse the match result */
    return (rule->flags & RULE_MAC_BLACKLIST) ? !found : found;
}

/* Check if ifname matches the rule */
static bool dns_mark_match_ifname(struct dns_mark_rule *rule, const unsigned char *ifname)
{
    struct dns_mark_ifname_entry *entry;
    bool found = false;

    /* If no ifname entries, consider it as matched*/
    if (atomic_read(&rule->ifname_count) == 0)
        return true;

    list_for_each_entry(entry, &rule->ifname_list, list) {
        if (strcmp(entry->ifname, ifname) == 0) {
            found = true;
            break;
        }
    }

    return found;
}

/* Check if domain matches the rule */
static bool dns_mark_match_domain_rule(struct dns_mark_rule *rule, const char *domain)
{
    struct dns_mark_domain_entry *entry = NULL;
    bool found = false;

    /* If no domain entries, consider it as matched when blacklist mode*/
    if (atomic_read(&rule->domain_count) == 0)
        return (rule->flags & RULE_DOMAIN_BLACKLIST) ? !found : found;

    list_for_each_entry(entry, &rule->domain_list, list) {
        if (dns_mark_match_domain(entry->domain_pattern, domain)) {
            found = true;
            break;
        }
    }

    /* If blacklist mode, reverse the match result */
    return (rule->flags & RULE_DOMAIN_BLACKLIST) ? !found : found;
}

/* Apply rules to packet */
u32 dns_mark_apply_rules(struct sk_buff *skb, struct dns_query_info *info)
{
    struct dns_mark_rule *rule = NULL;
    struct ethhdr *eth;
    u32 mark = 0;

    if (!skb || !info)
        return 0;

    /* Get ethernet header */
    eth = eth_hdr(skb);
    if (!eth)
        return 0;

    if (skb->dev->name[0] == '\0')
        return 0;

    if (strcmp(skb->dev->name, "lo") == 0 || strncmp(skb->dev->name, "lo:", 3) == 0)
        return 0;

    /* Check each rule */
    spin_lock(&rule_mgr.lock);
    list_for_each_entry(rule, &rule_mgr.rules, list) {
        /* Check if both MAC and domain match */
        if (dns_mark_match_ifname(rule, skb->dev->name) && dns_mark_match_mac(rule, eth->h_source) &&
                dns_mark_match_domain_rule(rule, info->domain)) {
            mark = rule->mark;
            break;
        }
    }
    spin_unlock(&rule_mgr.lock);

    return mark;
}

