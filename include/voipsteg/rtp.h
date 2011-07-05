//#include <map>

#ifndef _VOIPSTEG_RTP_H_
#define _VOIPSTEG_RTP_H_

extern "C" {
    #include <unistd.h>
}

//! RTP packet header. Every RTP packet has at least 12 bytes
typedef struct {
    int ver : 2,
        padding : 1,
        extension : 1,
        csrccount : 2,
        marker    : 1,
        payloadtype : 7,
        seq : 16;
    int timestamp;
    int ssrcid;
}__attribute__((packed)) vsrtp_rtphdr_t;

//! Access synchronization
//pthread_t rtpsesmutex__;
//pthread_t rtppacmutex__;

//! Currently available packets for use
//int queuedpackets__;

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

//! Initiazlie both queues and start capturing RTP packets
/*!
  Start filtering all UDP packets. Packets which destination and
  source are stored in __rtp_session collection are passed to
  additional queue. Other packets are ACCEPTed and passed forward.
  It also ACCEPT all RTP packages if trasmission has been finished.
 */
void* vsrtp_init(void*);

//! Thread function for accesing queue which contains RTP packets
/*!
  After capturing RTP packets going out side  here is
  a routine which sends them forward with correct delay
 */
void* vsrtp_capture(void*);

//! Called when new session is established. Control access to __rtp_sessions
/*!
  When session is created or ended a corresponding function is called. This
  function control access to global variables __rtp_sessions from multiple
  threads. Also iptables rules are created and deleted here.
 */
void vsrtp_session_add();
void vsrtp_session_del();
//vsrtp_session_t vsrtp_session_find(char* callid);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif
#endif /* _VOIPSTEG_RTP_H_  */
