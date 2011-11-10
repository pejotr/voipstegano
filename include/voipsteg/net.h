#include <netinet/in.h>

#ifndef _VOIPSTEG_NET_H_
#define _VOIPSTEG_NET_H_

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

#include <linux/stddef.h>
#include <linux/ip.h>
#include <linux/udp.h>

#define PACKET_SIZE 4096

//! Copy of packet
typedef struct net_packet_wrapper
{
    char buf[PACKET_SIZE];
    int  rv;
} packet_wrapper_t;

//! For retriving IP address of sender and receiver
struct iphdr* vsnet_parse_ip_hdr(int length, const char* buf);

//! Parse buffer to UDP packet if possible
struct udphdr* vsnet_parse_udp_hdr(int length, const char* buf);

//! Extracts IP adresses from IP header
int net_get_ip(int length, const char *pBuf, unsigned int *pOutSIp, unsigned int *pOutDIp);

//! Return ip addreses in human readable form
int net_get_ip_readable(int length, const char *pBuf, char *pOutSIp, char *pOutDIp);

int vsnet_get_port(int length, const char *pBuf, unsigned short *pOutSPort, unsigned short *pOutDPort);

const char* vsnet_skip_ip_hdr(int length, const char* buf);
const char* vsnet_skip_udp_hdr(int length, const char* buf);

//! Move pointer n bytes forward if possible
const char* vsnet_advance_n(int length, const char* buf, int n);

void net_ip2string(unsigned ip, char *pszOutIp);
void net_string2ip(const char* pszIp, unsigned *pOutIp);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif /* _VOIPSTEG_NET_H_ */
