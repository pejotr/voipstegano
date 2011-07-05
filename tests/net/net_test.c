#include <stdio.h>
#include "voipsteg/net.h"

char udppacket[] = {
    0x45, 0x00, 0x01, 0xdb, 0x00, 0x0e, 0x00, 0x00, 0x80, 0x11, 0x76,
    0x85, 0xc0, 0xa8, 0x01, 0xd7, 0xff, 0xff, 0xff, 0xff,
    0x04, 0x01, 0x20, 0x21, 0x01, 0xc7, 0xe9, 0x2c
};

int main()
{
    struct iphdr *ipheader = vsnet_parse_ip_hdr(256, udppacket);
    printf("Src. ip: %d\n", ipheader->saddr);
    printf("Dst. ip: %d\n", ipheader->daddr);
    printf("Version: %d\n", ipheader->version);
    printf("ihl: %d\n", ipheader->ihl);

    struct udphdr *udpheader = vsnet_parse_udp_hdr(265, udppacket);
    printf("Src. port = %d\n", ntohs(udpheader->source));
    printf("Dst. port = %d\n", ntohs(udpheader->dest));
    
    return 0;
}
