/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4:  */
/*
 * Copyright (C) 2010  Piotr Doniec, Warsaw University of Technology
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _VOIPSTEG_NETFILTER_H_
#define _VOIPSTEG_NETFILTER_H_


#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>
#include <libnetfilter_queue/libnetfilter_queue.h>

// TODO: malo intuicyjne -- do zmiany
#define FROM_LOCALNET   "--dport"
#define FROM_INTERNET   "--sport"

//TODO: Zmeinic na NF_ADD i NF_DEL
#define ADD "-A"
#define DEL "-D"
#define ANY "*"

#define NF_CHAIN_INPUT  "INPUT"
#define NF_CHAIN_OUTPUT "OUTPUT"
#define NF_CHAIN_FWD    "FORWARD"

extern int  __netfilter_errorno;

//! Queue description
typedef struct 
{
    struct nfq_handle   *pNfqHandle;
    int                 queuefd;
    int                 queueNum;
} queue_desc_t;

typedef struct
{
    char  szSrcPort[7],
          szDstPort[7];
    char  szSrcIp[16],
          szDstIp[16];
    char  szQueue[6];
    char  szChain[9];
} netfilter_rule_t;

//! Initialize new queue
/*!
 */
struct nfq_handle* netfilter_init_queue(const char *pszQueue, nfq_callback *cb);

//! Close opened queue
/*!
  @param qh Queue handler
 */
int netfilter_close_queue(int qh);

//! Translate error number to text message
const char* netfilter_error_msg(int errornum);

//! Manage iptables rules
/*!
    This function create an iptables rule from given data stored within netfilter_rule_t 
    structure. Depending on action this rule can be added or deleted from iptables.
    @param rule Rule to be created
    @param pszAction Action to be performed. Valid values are -A and -D. See defines ADD and DEL.
    @return Result of operation
 */
int netfilter_manage_rule(const netfilter_rule_t *rule, const char* pszAction);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif /* _VOIPSTEG_NETFILTER_H_  */
