#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
 
#include "voipsteg/sip/common.hxx"
#include "voipsteg/sip/out.hxx"
#include "voipsteg/sip/in.hxx"
 
void
signal_callback_handler(int signum)
{
    printf("Caught signal %d\n",signum);
    system("iptables -D OUTPUT -p udp --dport 5060 -j NFQUEUE --queue-num 0");

    exit(signum);
}

int main()
{
    signal(SIGINT, signal_callback_handler);

    system("iptables -A OUTPUT -p udp --dport 5060 -j NFQUEUE --queue-num 0");

    sip::init(NULL);    

    return EXIT_SUCCESS;
}

