// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * Domain Filter Module:Implementation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/file.h>
#include <net/sock.h>
#include <net/inet_sock.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_domainfilter.h>

// Below char works as wildcard (*), it can be used as part or whole domain
static const char WILDCARD = '%';

/*
 * Check if a given string is the ending substring of another.
 */
static bool endsWith(const char *host, const char *rule)
{
	size_t host_len = strlen(host);
	size_t rule_len = strlen(rule);

	if (host_len >= rule_len) {
		unsigned int off_set = host_len - rule_len;

		return strncmp(host + off_set, rule, rule_len) == 0;
	} else {
		return false;
	}
}

/*
 * Check if a given string is the beginning substring of another.
 */
static bool beginsWith(const char *host, const char *rule)
{
	size_t host_len = strlen(host);
	size_t rule_len = strlen(rule);

	if (host_len >= rule_len)
		return strncmp(host, rule, rule_len) == 0;
	else
		return false;
}

/*
 * Check if the given host matches the provided white/black list rules.
 */
static bool matchHost(const char *rule, const char *host)
{
	size_t rule_len = strlen(rule);

	if (rule_len == 1 && rule[0] == WILDCARD) // rule is *, means all hosts
		return true;
	if (rule[0] == WILDCARD) { // starts with *
		if (rule[rule_len - 1] == WILDCARD) { // also ends with *
			// get the substring between the '*'s
			char substrRule[XT_DOMAINFILTER_NAME_LEN];

			int ret = strscpy(substrRule,rule+1,rule_len-1);
			if (ret < 0)
				return false;
			if (strstr(host, substrRule))
				return true;
		} else { // only starts with *
			// remove * from beginning, so host must end if rule
			char substrRule[XT_DOMAINFILTER_NAME_LEN];

			int ret = strscpy(substrRule,rule+1,rule_len);
			if (ret < 0)
				return false;
			if (endsWith(host, substrRule))
				return true;
		}
	} else if (rule[rule_len - 1] == WILDCARD) { // only ends with '*'
		char substrRule[XT_DOMAINFILTER_NAME_LEN];

		int ret = strscpy(substrRule,rule,rule_len);
		if (ret < 0)
			return false;
		if (beginsWith(host, substrRule))
			return true;
	} else if (strlen(host) == rule_len &&
		   strcmp(host, rule) == 0) { // exact match
		return true;
	}
	return false;
}

static int domainfilter_check(const struct xt_mtchk_param *par)
{
	struct xt_domainfilter_match_info *info = par->matchinfo;

	if (strnlen(info->domain_name, sizeof(info->domain_name)) == sizeof(info->domain_name))
		return -EINVAL;
	if (!(info->flags & (XT_DOMAINFILTER_WHITE | XT_DOMAINFILTER_BLACK)))
		return -EINVAL;
	return 0;
}

static bool domainfilter_mt(const struct sk_buff *skb,
			    struct xt_action_param *par)
{
	const struct xt_domainfilter_match_info *info = par->matchinfo;
	struct sock *sk = skb_to_full_sk(skb);

	if (!sk)
		return false;

	// check domain name match
	if (SOCK_NPA_VENDOR_DATA_GET(sk)->domain_name != NULL &&
	    SOCK_NPA_VENDOR_DATA_GET(sk)->domain_name[0] != '\0') {
		return matchHost(info->domain_name,
				 SOCK_NPA_VENDOR_DATA_GET(sk)->domain_name);
	}

	// didn't match
	return false;
}

static struct xt_match domainfilter_mt_reg __read_mostly = {
	.name = "domainfilter",
	.revision = 1,
	.family = NFPROTO_UNSPEC,
	.checkentry = domainfilter_check,
	.match = domainfilter_mt,
	.matchsize = sizeof(struct xt_domainfilter_match_info),
	.hooks = (1 << NF_INET_LOCAL_OUT) | (1 << NF_INET_LOCAL_IN),
	.me = THIS_MODULE,
};

static int __init domainfilter_mt_init(void)
{
	return xt_register_match(&domainfilter_mt_reg);
}

static void __exit domainfilter_mt_exit(void)
{
	xt_unregister_match(&domainfilter_mt_reg);
}

module_init(domainfilter_mt_init);
module_exit(domainfilter_mt_exit);
MODULE_AUTHOR("Antonio Junqueira <antonio.n@samsung.com>");
MODULE_DESCRIPTION("Xtables: domain name matching");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_domainfilter");
MODULE_ALIAS("ip6t_domainfilter");
