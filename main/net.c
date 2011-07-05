#include "voipsteg/net.h"
#include <arpa/inet.h>

//! For retriving IP address of sender and receiver
struct iphdr* vsnet_parse_ip_hdr(int length, const char* buf)
{
    if((size_t)length >= sizeof(struct iphdr)) {
        struct iphdr *hdr = (struct iphdr*)buf;
        return hdr;
    }

    return NULL;
}

//! Parse buffer to UDP packet if possible
struct udphdr* vsnet_parse_udp_hdr(int length, const char* buf)
{
    struct iphdr *ipheader;
    struct udphdr *udpheader;
    int udphdrlen, iphdrlen, ippayloadlen;
    const char* ippayload;

    ipheader = vsnet_parse_ip_hdr(length, buf);
    
    if(NULL != ipheader) {
        iphdrlen = ipheader->ihl*4;
        udphdrlen = sizeof(struct udphdr);

        ippayloadlen = length - iphdrlen;
        ippayload = vsnet_advance_n(length, buf, iphdrlen);

        if(NULL != ippayload && ippayloadlen >= udphdrlen) {
            udpheader = (struct udphdr*)(ippayload);
            return udpheader;
        }
    }

    return NULL;
}

int net_get_ip(int length, const char *pBuf, unsigned int *pOutSIp, unsigned int *pOutDIp)
{
    struct iphdr* hdr = vsnet_parse_ip_hdr(length, pBuf);

    if( NULL == hdr )
    {
        return -1;
    }

    *pOutSIp = ntohl(hdr->saddr);
    *pOutDIp = ntohl(hdr->daddr);

    return 1;
}

int net_get_ip_readable(int length, const char *pBuf, char *pOutSIp, char *pOutDIp)
{
    struct iphdr* hdr = vsnet_parse_ip_hdr(length, pBuf);
    unsigned int saddr, daddr;
    int res;
    
    if( hdr == NULL )
    {
        return -1;
    }

    saddr = hdr->saddr;
    daddr = hdr->daddr;

    inet_ntop(AF_INET, (const void*)&saddr, pOutSIp, 15);
    inet_ntop(AF_INET, (const void*)&daddr, pOutDIp, 15);

    return 1;
}

void net_ip2string(unsigned int ip, char *pszOutIp)
{
    unsigned int hip = ntohl(ip);
    inet_ntop(AF_INET, (const void*)&hip, pszOutIp, 15);
}

void net_string2ip(const char* pszIp, unsigned *pOutIp)
{
    unsigned temp;

    inet_pton(AF_INET, pszIp, &temp);
    *pOutIp = htonl(temp);
}

int vsnet_get_port(int length, const char *pBuf, unsigned short *pOutSPort, unsigned short *pOutDPort)
{
    struct udphdr *hdr = vsnet_parse_udp_hdr(length, pBuf);
    
    if( hdr == NULL )
    {
        return -1;
    }

    *pOutSPort = hdr->source;
    *pOutDPort = hdr->dest;
    return 1;
}

const char* vsnet_skip_ip_hdr(int length, const char* buf)
{
    return NULL;
}

const char* vsnet_skip_udp_hdr(int length, const char* buf)
{
    return NULL;
}

const char* vsnet_advance_n(int length, const char* buf, int n)
{
    const char *newbuf = buf;

    if((size_t)length >= n) {
        return (newbuf + n);
    }
    
    return NULL;
}
