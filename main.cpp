#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "config.h"

#include "voipsteg/logger.h" 
#include "voipsteg/config.h"
#include "voipsteg/databank.h"
#include "voipsteg/sip/common.hxx"
#include "voipsteg/rtp/rtp.hxx"
 
void
signal_callback_handler(int signum)
{
#ifdef SEND
    db_dispose();
#endif

    system("iptables -D OUTPUT -p udp --dport 5060 -j NFQUEUE --queue-num 0");
    system("iptables -D INPUT -p udp --sport 5060 -j NFQUEUE --queue-num 1");

    exit(signum);
}

int main()
{
    int ret; 
    pthread_t sip, rtp;
    void *status;

    signal(SIGINT, signal_callback_handler);
    vsconf_readconfig("config.xml");

#ifdef MODULE_SENDER
    db_init_data();
    netfilter_rule_t sipIn  = { "5060", ANY, ANY, ANY, "1", "INPUT"}, 
                     sipOut = { "5060", ANY, ANY, ANY, "0", "OUTPUT"};

    SYS_LOG(E_NOTICE, "Starting SENDER");
    system("iptables -A INPUT -p udp --sport 5060 -j NFQUEUE --queue-num 1");
    netfilter_manage_rule(&sipOut, ADD);
#else
    SYS_LOG(E_NOTICE, "Starting REVEIVER");

    db_init_data();
    netfilter_rule_t sipIn  = { "5060", ANY, ANY, ANY, "1", "INPUT"}, 
                     sipOut = { "5060", ANY, ANY, ANY, "0", "OUTPUT"};

    netfilter_manage_rule(&sipIn, ADD);
    netfilter_manage_rule(&sipOut, ADD);
//    netfilter_add_rule("0", "5060", "INPUT", FROM_INTERNET);
//    netfilter_add_rule("1", "5060", "OUTPUT", FROM_LOCALNET);
#endif
//    pthread_create(&rtp, NULL, rtp::init, NULL);
    sip::init(NULL);   

//    pthread_join(rtp, &status); 


    return EXIT_SUCCESS;
}
