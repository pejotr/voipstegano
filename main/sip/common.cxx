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

#include "voipsteg/sip/common.hxx"
#include "voipsteg/sip/inout.hxx"

extern "C" {
    #include "voipsteg/utils.h"
    #include "voipsteg/logger.h"
}

using namespace sip;

sip::session_map_t  sip::gSessionMap;
pthread_mutex_t     sip::gSessionMapMutex;
pj_pool_t           *sip::__sipPool;
int                 *sip::__queueStatus;

//! Private declarations
namespace
{
    pjsip_endpoint *mSipEndpoint;
    pj_caching_pool mSipCachingPool;
    pthread_mutex_t mQueueStatusMux;
    
    int init_pjsip();
    struct pjsip_msg* parse_msg(char* buf, int length);
}

//! Initialize SIP capturing system
void* sip::init(void *pParams)
{
    int joinres1, joinres2;
    void *status;
    pthread_t thread_send, thread_recv;
    const vsconf_value_t *sipqueuea = vsconf_get("sip-queuenum-a"),
                         *sipqueueb = vsconf_get("sip-queuenum-b"), 
                         *sipporta  = vsconf_get("sip-port-a"), 
                         *sipportb  = vsconf_get("sip-port-b"),
                         *queueup   = vsconf_get("queue-upnum"),
                         *queuelo   = vsconf_get("queue-lonum");
    int queueCnt = queueup->value.numval - queuelo->value.numval;

    module_conf_t *config_send = new module_conf_t,
                  *config_recv = new module_conf_t;

    init_pjsip();

    pthread_mutex_init(&gSessionMapMutex, NULL);

    __queueStatus       = new int[queueCnt];
    pthread_mutex_init(&mQueueStatusMux, NULL);
    memset(__queueStatus, 0x00, queueCnt*sizeof(int));

    if( sipqueuea && sipqueueb && sipporta && sipportb )
    {
        config_send->pszQueue = sipqueuea->value.strval;
        config_send->pszPort  = sipporta->value.strval;
        config_send->dirInd   = OUTGOING;

        config_recv->pszQueue = sipqueueb->value.strval;
        config_recv->pszPort  = sipportb->value.strval;
        config_recv->dirInd   = INCOMMING;
        pthread_create(&thread_send, NULL, sip::inout::init, static_cast<void*>(config_send));
        pthread_create(&thread_recv, NULL, sip::inout::init , static_cast<void*>(config_recv));
    }
    else
    {
        SYS_LOG(E_ERROR, "sip::init: Malformed configuration file");
        return NULL; // TODO: Think about it !!!
    }

    SYS_LOG(E_DEBUG, "Joining threads");

    joinres1 = pthread_join(thread_send, &status);
    joinres2 = pthread_join(thread_recv, &status);
}

int sip::dispatch_packet(struct nfq_q_handle *pQh, struct nfgenmsg *pNfmsg, struct nfq_data *pNfa, 
                    void *pData, msg_dispatch_func funHandler)
{
    struct nfqnl_msg_packet_hdr *ph;
    unsigned char* payload;
    int length;
    int id;

    ph = nfq_get_msg_packet_hdr(pNfa);
    if(ph) {
        id = ntohl(ph->packet_id);
    }

    if( (length = nfq_get_payload(pNfa, &payload) ) <= 0 ) {
        SYS_LOG(E_ERROR, "<dispatch_packet> Error while getting packet payload")
        return nfq_set_verdict(pQh, id, NF_DROP, 0, NULL);
    }

    struct pjsip_msg* msg = parse_msg((char*)payload, length);
    if(msg != NULL) {

        funHandler(msg, (const char*)payload, length);

        // dokumentacja biblioteki stwierdza jednoznacznie ze zwalnianie zaalokowanej
        // przy pomocy pool pamieci nie jest konieczne
    }

    //! We accept all SIP packets 
    return nfq_set_verdict(pQh, id, NF_ACCEPT, 0, NULL);
}

//! Extract Call-ID from SIP essage
const std::string sip::get_callid(struct pjsip_msg *pMsg)
{
    pjsip_hdr *head = &(pMsg->hdr),
              *iter = pMsg->hdr.next;

    while( iter != head ) {
        if(iter->type == PJSIP_H_CALL_ID) {
            pjsip_cid_hdr* callid_hdr = (pjsip_cid_hdr*)iter;
            std::string callid(callid_hdr->id.ptr, (long)callid_hdr->id.slen);
            //SYS_LOG(E_DEBUG, "get_callid: Extracted Call-ID %s", callid.c_str())
            return callid;
        }
        iter = iter->next;
    }

    return std::string();
}

//! Extract media ports from SDP message
short sip::get_mediaport(struct pjsip_msg *pMsg)
{
    pjmedia_sdp_session*    session;
    pj_status_t             status;

    status = pjmedia_sdp_parse(sip::__sipPool, (char *)pMsg->body->data, pMsg->body->len, &session);

    if( status == PJ_SUCCESS )
    {
        SYS_LOG(E_DEBUG, "get_mediaport: Media Description count: %d", session->media_count);
        for(int i = 0; i < session->media_count; ++i )
        {
            if( strncmp(session->media[i]->desc.media.ptr, "audio", 5) == 0 )
            {
                SYS_LOG(E_DEBUG, "get_mediaport: Port for this session %d", session->media[i]->desc.port);
                return session->media[i]->desc.port;
            }
        }
    }

    SYS_LOG(E_WARNING, "get_mediaport: Unable to determine media port");
    return -1;

}

const std::string sip::get_connaddr(struct pjsip_msg *pMsg)
{
    pjmedia_sdp_session*    session;
    pj_status_t             status;

    status = pjmedia_sdp_parse(sip::__sipPool, (char *)pMsg->body->data, pMsg->body->len, &session);

    if( status == PJ_SUCCESS && session->conn != NULL )
    {
        const std::string connaddr(session->conn->addr.ptr, session->conn->addr.slen);
        return connaddr; 
    }
    
    SYS_LOG(E_WARNING, "get_connaddress: Unable to determine connection address");
    return std::string();
}

session_t* const sip::find_session (struct pjsip_msg *pSipMsg)
{
    char           digest[16];
    std::string    callidhash;
    std::string    callid = get_callid(pSipMsg);
    session_map_iter_t i;

    if( callid.empty() )
    {
        SYS_LOG(E_WARNING, "find_sessions: Call-ID header not found in SIP message");
        return NULL;
    }

    utils_md5hash(callid.c_str(), digest);
    callidhash.append(digest, 16);
    
    i = gSessionMap.find(callidhash);
    if( i == gSessionMap.end() )
    {
        SYS_LOG(E_WARNING, "<find_session> No such session");
        return NULL;
    }
    else
    {
        //SYS_LOG(E_DEBUG, "<find_session>: Session found");
        return &gSessionMap[callidhash];
    }
}

const std::string sip::callid_md5hash(struct pjsip_msg *pSipMsg)
{
    char           digest[16];
    char           digest_string[32] = {0x00, };
    std::string    callid = get_callid(pSipMsg);
    std::string    callidhash;

    
    if( callid.empty() )
    {
        SYS_LOG(E_WARNING, "<callid_md5hash> Call-ID header not found in SIP message");
        return NULL; 
    }

    utils_md5hash(callid.c_str(), digest);
    callidhash.append(digest, 16);

    return callidhash;
}

///////////////////////////////////////////////////////////////////////////////////////
//////// PRIVATE
///////////////////////////////////////////////////////////////////////////////////////
namespace 
{

int init_pjsip()
{
    pj_status_t status;

    status = pj_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    status = pjlib_util_init();
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);

    pj_caching_pool_init(&mSipCachingPool, NULL, 1024*1024);
    __sipPool = pj_pool_create(&mSipCachingPool.factory, "voipsteg_sip_pool", 1024, 1024, NULL);

    status = pjsip_endpt_create(&mSipCachingPool.factory, "voipsteg_endpt", &mSipEndpoint);
    PJ_ASSERT_RETURN(status == PJ_SUCCESS, 1);
    
    if( sip::__sipPool == NULL ) {
        return -1;
    }
}

struct pjsip_msg* parse_msg(char* buf, int length)
{
    struct  iphdr  *ipheader  = vsnet_parse_ip_hdr(length, buf);
    struct  udphdr *udpheader = vsnet_parse_udp_hdr(length, buf);
    int     siplen;
    char    *sippayload;
    pjsip_parser_err_report error;

    if( NULL != udpheader &&  NULL != ipheader ) {
        siplen = length - sizeof(struct udphdr) - ipheader->ihl * 4;
        sippayload = buf + sizeof(struct udphdr) + ipheader->ihl * 4;

        pj_list_init(&error);
            
        return pjsip_parse_msg(__sipPool, sippayload, siplen, &error);
    }
    return NULL;
}


}
