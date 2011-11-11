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

#ifndef _VOIPSTEG_SIP_COMMON_
#define _VOIPSTEG_SIP_COMMON_

#include <map>
#include <list>
#include <string>

extern "C" 
{
    #include <pthread.h>
    #include <netinet/in.h>
    #include <linux/netfilter.h>
    #include <libnetfilter_queue/libnetfilter_queue.h>

    #include <pjsip.h>
    #include <pjlib-util.h>
    #include <pjlib.h>
    #include <pjmedia.h>

    #include "voipsteg/config.h"
    #include "voipsteg/netfilter.h"
}

#define MAP_SYNC_B  pthread_mutex_lock  (&gSessionMapMutex);
#define MAP_SYNC_E  pthread_mutex_unlock(&gSessionMapMutex);

namespace sip {

//! Session status enumerator
enum SESSION_STATUS  { INVITE1, AUTHREQ, ACK1, INVITE2, OK1, ACK2, ESTABLISHED, BYE, SRVUNAVAIL, REINVITE, REINVITEOK1 };

//! Module direction indicator
enum QUEUE_DIRECTION { UNDEF, INCOMMING_PACKETS, OUTGOING_PACKETS };

//! Internal session description
typedef struct sip_common_session {
    unsigned int   ipremote,
                   iplocal;
    unsigned short portremote,
                   portlocal;
    enum SESSION_STATUS status;

    unsigned int rtpInstanceId;
} session_t;

//! Handlers tyle definition
typedef int (*msg_dispatch_func)(struct pjsip_msg *pSipMsg, const char *pBuf, int length);
typedef int (*msg_handler_func)(session_t *session,struct pjsip_msg *pSipMsg, const char *pBuf, int length);

//! Configuration structure for "in" and "out" submodules
typedef struct {
    const char *pszQueue;
    const char *pszPort;
    QUEUE_DIRECTION dirInd;
} module_conf_t;

typedef std::map<std::string, session_t>                        session_map_t;
typedef std::map<std::string, session_t>::iterator              session_map_iter_t;
typedef std::map<std::string, std::list<pjmedia_sdp_media> >    session_media_map_t;
typedef std::list<pjmedia_sdp_media>                            media_list_t;

extern session_map_t        gSessionMap;
extern pthread_mutex_t      gSessionMapMutex;
extern pj_pool_t            *__sipPool;
extern int                  *__queueStatus;

//! Initialize SIP capturing system
void* init(void *pParams);

//! Check if message is correct and dispatch to handler
int dispatch_packet(struct nfq_q_handle *pQh, struct nfgenmsg *pNfmsg, struct nfq_data *pNfa, 
                    void *pData, msg_dispatch_func funHnd);

//@{
/** Helper functions extracting particular information from SIP or SDP message */
//! Extract call id header from SIP message
const std::string get_callid(struct pjsip_msg *pMsg);
//! Extract connction address from SDP message
const std::string get_connaddr(struct pjsip_msg *pMsg);
//! Extract media port from SDP message
short get_mediaport(struct pjsip_msg *pMsg);
//@}

session_t* const    find_session  (struct pjsip_msg *pSipMsg);

const std::string   callid_md5hash(struct pjsip_msg *pSipMsg);

//@{
/** Common functions used to handle packetes in both modules */
//! Function called whenever INVITE message was received for non-existing session
bool new_session(const std::string& callidhash, struct pjsip_msg *pSipMsg, const char *pBuf, int length, session_t *pRet);
//@}

//! Compare two session. Used to prevent unnessesary modyfication of iptables rules
/*!
    @return TRUE - when sessions are identical
 */
inline bool cmp_sessions(const session_t *s1, const session_t *s2)
{
   return ( s1->ipremote == s2->ipremote ) && ( s1->iplocal == s2->iplocal ) &&
          ( s1->portremote == s2->portremote ) && (s1->portlocal == s2->portlocal );
}

}

#endif /* _VOIPSTEG_SIP_COMMON_ */
