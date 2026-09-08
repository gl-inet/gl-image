#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/if_vlan.h>
#include <linux/udp.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/ratelimit.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include "dns_mark.h"

/* DNS header structure */
struct dns_header {
    __u16 id;
    __u16 flags;
    __u16 qdcount;
    __u16 ancount;
    __u16 nscount;
    __u16 arcount;
} __attribute__((packed));

#define IS_IPV6(skb) (!skb_vlan_tag_present(skb) && skb->protocol == htons(ETH_P_IPV6))

int parse_dns_query(struct sk_buff *skb, struct udphdr *udph, struct dns_query_info *info)
{
    int offset = 0;
    int data_len, label_len;
    unsigned char *ptr;
    unsigned char *data;
    struct dns_header *dnsh;

    /* Get DNS header */
    dnsh = (struct dns_header *)(skb_transport_header(skb) + sizeof(struct udphdr));
    data = (unsigned char *)dnsh + sizeof(struct dns_header);
    data_len = ntohs(udph->len) - sizeof(struct udphdr) - sizeof(struct dns_header);

    if (data_len <= 0 || data_len > DNS_MAX_DOMAIN_LENGTH)
        return -EINVAL;

    /* Check if it's a standard query */
    if (ntohs(dnsh->qdcount) != 1)
        return -EINVAL;

    /* Parse domain name */
    ptr = (unsigned char *)info->domain;
    while (offset < data_len) {
        label_len = data[offset++];
        if (label_len == 0)
            break;

        if (label_len > DNS_MAX_LABEL_LENGTH)
            return -EINVAL;

        if (ptr != (unsigned char *)info->domain)
            *ptr++ = '.';

        if ((ptr - (unsigned char *)info->domain) + label_len >= DNS_MAX_DOMAIN_LENGTH)
            return -EINVAL;

        memcpy(ptr, &data[offset], label_len);
        ptr += label_len;
        offset += label_len;
    }

    *ptr = '\0';
    info->domain_len = ptr - (unsigned char *)info->domain;

    return 0;
}

/* Parse DNS query from skb */
static int parse_dns_query_tcp(struct sk_buff *skb, struct tcphdr *tcph, struct dns_query_info *info)
{
    int offset = 0;
    int data_len;
    unsigned char *ptr;
    struct dns_header dnsh;
    __u16 dns_len;
    unsigned char label_buf[DNS_MAX_LABEL_LENGTH];
    int transport_offset = skb_transport_offset(skb);
    int header_offset = transport_offset + tcph->doff * 4;

    /* Calculate available data length and verify minimum length */
    data_len = skb->len - header_offset;

    /* ---- Try normal TCP DNS with 2-byte length prefix ---- */
    do {
        int msg_data_len;
        if (data_len < 2)
            break; /* fall back */

        if (skb_copy_bits(skb, header_offset, &dns_len, 2) < 0)
            break; /* fall back */
        dns_len = ntohs(dns_len);
        if (dns_len + 2 > data_len || dns_len < sizeof(struct dns_header))
            break; /* fall back */

        /* Get DNS header */
        if (skb_copy_bits(skb, header_offset + 2, &dnsh, sizeof(struct dns_header)) < 0)
            break; /* fall back */

        /* Standard query: QR=0 and OPCODE=0 */
        if ((ntohs(dnsh.flags) & 0x8000) || ((ntohs(dnsh.flags) >> 11) & 0xF))
            break; /* fall back */

        if (ntohs(dnsh.qdcount) != 1)
            break; /* fall back */

        /* Parse domain name */
        ptr = (unsigned char *)info->domain;
        offset = 0;
        msg_data_len = dns_len - sizeof(struct dns_header);
        /* track whether we saw the zero-length label terminator */
        {
            int zero_label_found = 0;

            while (offset < msg_data_len) {
                unsigned char len;
                if (skb_copy_bits(skb, header_offset + 2 + sizeof(struct dns_header) + offset, &len, 1) < 0)
                    return -EINVAL;

                if (len == 0) {
                    zero_label_found = 1;
                    break;
                }

                if (len > DNS_MAX_LABEL_LENGTH)
                    return -EINVAL;

                if (ptr != (unsigned char *)info->domain)
                    *ptr++ = '.';

                if ((ptr - (unsigned char *)info->domain) + len >= DNS_MAX_DOMAIN_LENGTH)
                    return -EINVAL;

                /* ensure label bytes stay within declared DNS message data length */
                if (offset + 1 + len > msg_data_len)
                    return -EINVAL;

                if (skb_copy_bits(skb, header_offset + 2 + sizeof(struct dns_header) + offset + 1, label_buf, len) < 0)
                    return -EINVAL;

                memcpy(ptr, label_buf, len);
                ptr += len;
                offset += len + 1;
            }

            /* require zero-length label terminator and room for QTYPE/QCLASS */
            if (!zero_label_found)
                return -EINVAL;
            if (offset + 1 + 4 > msg_data_len)
                return -EINVAL;

            *ptr = '\0';
            info->domain_len = ptr - (unsigned char *)info->domain;
            return 0; /* success via normal path */
        }
    } while (0);

    /* ---- Best-effort fallback: scan current payload without 2-byte prefix ---- */
    {
        int scan_off;
        for (scan_off = 0; scan_off < 4; scan_off++) {
            int hdr_off = header_offset + scan_off;
            int avail = skb->len - hdr_off;
            int pos;
            unsigned char len = 0;
            int zero_label_found = 0;

            /* Need at least DNS header + root label + QTYPE/QCLASS */
            if (avail < (int)sizeof(struct dns_header) + 1 + 4)
                continue;

            if (skb_copy_bits(skb, hdr_off, &dnsh, sizeof(struct dns_header)) < 0)
                continue;

            /* Standard query: QR=0 and OPCODE=0; and one question */
            if ((ntohs(dnsh.flags) & 0x8000) || ((ntohs(dnsh.flags) >> 11) & 0xF))
                continue;
            if (ntohs(dnsh.qdcount) != 1)
                continue;

            /* Parse QNAME */
            ptr = (unsigned char *)info->domain;
            pos = hdr_off + sizeof(struct dns_header);
            while (pos < hdr_off + avail) {
                if (skb_copy_bits(skb, pos, &len, 1) < 0)
                    goto next_scan; /* try next offset */
                pos++;

                if (len == 0) {
                    zero_label_found = 1;
                    break; /* end of QNAME */
                }

                if (len > DNS_MAX_LABEL_LENGTH)
                    goto next_scan;

                if (ptr != (unsigned char *)info->domain)
                    *ptr++ = '.';

                if ((ptr - (unsigned char *)info->domain) + len >= DNS_MAX_DOMAIN_LENGTH)
                    goto next_scan;

                if (pos + len > hdr_off + avail)
                    goto next_scan;

                if (skb_copy_bits(skb, pos, label_buf, len) < 0)
                    goto next_scan;
                memcpy(ptr, label_buf, len);
                ptr += len;
                pos += len;
            }

            /* must have found zero label and room for QTYPE/QCLASS */
            if (!zero_label_found)
                goto next_scan;
            if (pos + 4 > hdr_off + avail)
                goto next_scan;

            *ptr = '\0';
            info->domain_len = ptr - (unsigned char *)info->domain;
            return 0; /* success via fallback */

next_scan:
            ;
        }
    }

    return -EINVAL;
}

/* Parse DNS query from skb for both IPv4 and IPv6 */
static int __dns_mark_parse_query(struct sk_buff *skb, struct dns_query_info *info,
                                  u8 protocol)
{
    int ret;
    struct udphdr *udph;
    struct tcphdr *tcph;

    if (!skb || !info)
        return -EINVAL;

    /* Check protocol */
    if (protocol == IPPROTO_UDP) {
        /* Get UDP header */
        udph = udp_hdr(skb);
        if (!udph)
            return -EINVAL;

        /* Check if it's DNS query (destination port 53) */
        if (ntohs(udph->dest) != 53)
            return -EINVAL;

        ret = parse_dns_query(skb, udph, info);
    } else if (protocol == IPPROTO_TCP) {
        /* Get TCP header */
        tcph = tcp_hdr(skb);
        if (!tcph)
            return -EINVAL;

        /* Check if it's DNS query (destination port 53) */
        if (!dns_mark_port_is_dns(ntohs(tcph->dest)))
            return -EINVAL;

        /* Only parse established connections and packets with payload */
        if (tcph->syn || tcph->fin || tcph->rst || !skb->len)
            return -EINVAL;

        ret = parse_dns_query_tcp(skb, tcph, info);
    } else {
        return -EINVAL;
    }

    return ret;
}

/* Parse DNS query from IPv4 packet */
int dns_mark_parse_query(struct sk_buff *skb, struct dns_query_info *info)
{
    struct iphdr *iph;

    if (!skb || !info)
        return -EINVAL;

    /* Get IP header */
    iph = ip_hdr(skb);
    if (!iph)
        return -EINVAL;

    return __dns_mark_parse_query(skb, info, iph->protocol);
}

/* Parse DNS query from IPv6 packet */
int dns_mark_parse_query6(struct sk_buff *skb, struct dns_query_info *info)
{
    struct ipv6hdr *ip6h;

    if (!skb || !info)
        return -EINVAL;

    if (!IS_IPV6(skb))
        return -EINVAL;

    /* Get IPv6 header */
    ip6h = ipv6_hdr(skb);
    if (!ip6h)
        return -EINVAL;

    return __dns_mark_parse_query(skb, info, ip6h->nexthdr);
}
