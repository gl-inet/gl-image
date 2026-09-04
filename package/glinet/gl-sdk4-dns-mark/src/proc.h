#ifndef _DNS_MARK_PROC_H
#define _DNS_MARK_PROC_H

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include "dns_mark.h"

struct dns_mark_rule_proc {
    struct proc_dir_entry *rule_dir;       /* /proc/dns_mark/ruleX */
    struct proc_dir_entry *ifnames;        /* /proc/dns_mark/ruleX/ifnames */
    struct proc_dir_entry *macs;           /* /proc/dns_mark/ruleX/macs */
    struct proc_dir_entry *domains;        /* /proc/dns_mark/ruleX/domains */
    struct proc_dir_entry *mac_flag;       /* /proc/dns_mark/ruleX/mac_flag */
    struct proc_dir_entry *domain_flag;    /* /proc/dns_mark/ruleX/domain_flag */
    struct proc_dir_entry *mark;           /* /proc/dns_mark/ruleX/mark */
    struct proc_dir_entry *dump;           /* /proc/dns_mark/ruleX/dump */
    struct proc_dir_entry *clear;          /* /proc/dns_mark/ruleX/clear */
    struct dns_mark_rule *rule;            /* pointer to the actual rule */
};

struct dns_mark_proc_dir {
    struct proc_dir_entry *proc_root;      /* /proc/dns_mark */
    struct proc_dir_entry *proc_dump;      /* /proc/dns_mark/dump */
    struct proc_dir_entry *proc_clear;     /* /proc/dns_mark/clear */
    struct proc_dir_entry *dump;           /* /proc/dns_mark/dump */
    struct proc_dir_entry *clear;          /* /proc/dns_mark/clear */
    struct proc_dir_entry *create;         /* /proc/dns_mark/create */
    struct proc_dir_entry *debug;          /* /proc/dns_mark/debug */
    struct proc_dir_entry *intercept_forward; /* /proc/dns_mark/intercept_forward */
};

/* Global variables */
extern struct dns_mark_proc_dir proc_dir;
extern char *dns_mark_write_domains_pending_buf;
extern size_t dns_mark_write_domains_pending_len;

/* Function declarations */
int dns_mark_proc_init(void);
void dns_mark_proc_cleanup(void);
int dns_mark_create_rule_proc(struct dns_mark_rule *rule, int rule_id);
void dns_mark_remove_rule_proc(struct dns_mark_rule_proc *rule_proc);

#endif /* _DNS_MARK_PROC_H */
