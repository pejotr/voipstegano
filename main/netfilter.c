#include <pthread.h>
#include "voipsteg/netfilter.h"
#include "voipsteg/net.h"

int __netfilter_errorno;

static const char error_msg[6][255] = {
    "SUCCESS"                 ,
    "nfq_open queue failed"   ,
    "nfq_unbind_pf failed"    ,
    "nfq_bind_pf failed"      ,
    "nfq_create_queue failed" ,
    "nfq_set_mode failed"    
};

static int             _mutexinitialized;
static pthread_mutex_t _queueinitMutex;

struct nfq_handle* netfilter_init_queue(int queueNum, nfq_callback *cb)
{
    int queuenum, portnum;
    struct nfq_handle *nfq_handle;
    struct nfq_q_handle *queue_handle;
    struct nfln_handle *netlink_handle;

    queuenum = queueNum;
    __netfilter_errorno = 0;

    if( !_mutexinitialized )
    {
        pthread_mutex_init(&_queueinitMutex, NULL);
        _mutexinitialized = 1;
    }

    pthread_mutex_lock(&_queueinitMutex);
    if(!(nfq_handle = nfq_open())) {
        __netfilter_errorno = 1;
        nfq_handle = NULL;
        goto niq_end;
    }

    if(nfq_unbind_pf(nfq_handle, AF_INET) < 0 ) {
        __netfilter_errorno = 2;
        nfq_handle = NULL;
        goto niq_end;
    }

    if(nfq_bind_pf(nfq_handle, AF_INET) < 0) {
        __netfilter_errorno = 3;
        nfq_handle = NULL;
        goto niq_end;
    }

    queue_handle = nfq_create_queue(nfq_handle, queuenum, cb, NULL);
    if(!queue_handle) {
        __netfilter_errorno = 4;
        nfq_handle = NULL;
        goto niq_end;
    }

    if(nfq_set_mode(queue_handle, NFQNL_COPY_PACKET, 0xffff) < 0 ) {
        __netfilter_errorno = 5;
        nfq_handle = NULL;
        goto niq_end;
    }

niq_end:
    pthread_mutex_unlock(&_queueinitMutex);
    return nfq_handle;
}

//! Creates new netfiler rule
netfilter_rule_t netfilter_create_rule(unsigned int srcIp, 
                                       unsigned short srcPort, 
                                       unsigned int dstIp, 
                                       unsigned short dstPort, int queueNum, 
                                       const char* szChain)
{
    char szSrcIp[16] = { 0x00, }, 
         szDstIp[16] = { 0x00, };
    netfilter_rule_t rule; 

    net_ip2string(srcIp, szSrcIp);
    net_ip2string(dstIp ,szDstIp);

    sprintf(rule.szSrcPort, "%d", srcPort);
    sprintf(rule.szDstPort, "%d", dstPort);
    sprintf(rule.szSrcIp,   "%s", szSrcIp);
    sprintf(rule.szDstIp,   "%s", szDstIp);
    sprintf(rule.szQueue,   "%d", queueNum  );
    strcpy(rule.szChain,    szChain);

    return rule;
}

//! Add new rule to iptables
int netfilter_manage_rule(const netfilter_rule_t *rule, const char* pszAction)
{
    char command[128] = { 0x00, };

    sprintf(command, "iptables %s %s -p udp",pszAction, rule->szChain);

    if( strcmp(rule->szSrcPort, ANY) != 0 )
    {
        sprintf(command, "%s --sport %s", command, rule->szSrcPort);
    }

    if( strcmp(rule->szDstPort, ANY) != 0 )
    {
        sprintf(command, "%s --dport %s", command, rule->szDstPort);
    }

    if( strcmp(rule->szSrcIp, ANY) != 0 )
    {
        sprintf(command, "%s -s %s", command, rule->szSrcIp);
    }

    if( strcmp(rule->szDstIp, ANY) != 0 )
    {
        sprintf(command, "%s -d %s", command, rule->szDstIp);
    }

    sprintf(command, "%s -j NFQUEUE --queue-num %s", command, rule->szQueue);

    printf(">>>>>> %s \n", command);
    system(command);

    return 1;

}
