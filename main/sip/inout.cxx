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

#include <utility>

#include "voipsteg/sip/common.hxx"
#include "voipsteg/sip/inout.hxx"

extern "C"
{
    #include <assert.h>
    #include "voipsteg/net.h"
    #include "voipsteg/utils.h"
}

using namespace sip;
using namespace inout;

sigc::signal<void, session_t*> sip::inout::sig_init;
sigc::signal<void, session_t*> sip::inout::sig_bye;
sigc::signal<void, session_t*, session_t*> sip::inout::sig_reinvite;

namespace sip { namespace inout {

//! Declaration hidden for user
namespace 
{

std::map<pjsip_method_e, msg_handler_func> mReqMessageHandler;
std::map<int, msg_handler_func> mStatusMessageHandler;
std::map<pthread_t, QUEUE_DIRECTION> mThreadDirection;
std::string directionInd;

//@{
/** Request handlers */
int invite (session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length);
int ack    (session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length);
int bye    (session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length);
//@}

//@{
/** Status handlers */
int ok200  (session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length);
int par407 (session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length);
int srvunavail503 (session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length);
//@}

//! Invite message handler
/*!
    Called only when INVITE message was received and there was not any session
    with same callid already registred
*/
int invite1(session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length);

//! Invoked when second invite was sent with additional information such as auth
int reinvite(session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length);

}

void* main_routine(void *pParam)
{
    int             err = 0;
    int             rv, fd;
    char            buf[4096];
    struct nfq_handle *pNfqHandle;
    pj_thread_desc  desc;
    pj_thread_t     *this_thread;
    pj_status_t     status;
    module_conf_t   *conf = static_cast<module_conf_t*>(pParam);
    pthread_t       me    = pthread_self();
    
    SYS_LOG(E_NOTICE, "Starting sip::inout thread. Queue number is %s and port is %s", conf->pszQueue, conf->pszPort);    

    status = pj_thread_register("in-thread", desc, &this_thread);

    if( status != PJ_SUCCESS) 
    {
        SYS_LOG(E_ERROR, "Error in pj_thread_register");
        delete conf;
        return NULL;
    }

    mThreadDirection[me] = conf->dirInd;

    mStatusMessageHandler.insert( std::make_pair(200, ok200)         ); 
    mStatusMessageHandler.insert( std::make_pair(407, par407)        );
    mStatusMessageHandler.insert( std::make_pair(503, srvunavail503) ); 

    mReqMessageHandler.insert( std::make_pair(PJSIP_INVITE_METHOD, invite) ); 
    mReqMessageHandler.insert( std::make_pair(PJSIP_ACK_METHOD   , ack   ) ); 
    mReqMessageHandler.insert( std::make_pair(PJSIP_BYE_METHOD   , bye   ) );

    pNfqHandle = netfilter_init_queue(conf->pszQueue, inout::handle_queue);
    fd = nfq_fd(pNfqHandle);

    delete conf;

    assert(pNfqHandle != NULL);
    assert(fd != -1);

    while((rv = recv(fd, buf, sizeof(buf), 0)) && rv >= 0) 
    {
        nfq_handle_packet(pNfqHandle, buf, rv);
    }
}

int handle_queue(struct nfq_q_handle *pQh, struct nfgenmsg *pNfmsg, struct nfq_data *pNfa, void *pData)
{
    return dispatch_packet(pQh, pNfmsg, pNfa, pData, handle_sip_msg);
}


int handle_sip_msg(struct pjsip_msg* pSipMsg, const char *pBuf, int length)
{
    int             code;
    char            requri[512];
    std::string     method, reason;
    pjsip_method_e  method_id;
    session_t       *session;

    assert(pSipMsg != NULL);
    switch(pSipMsg->type)
    {
    case PJSIP_REQUEST_MSG:
    {
        method_id = pSipMsg->line.req.method.id;
        std::map<pjsip_method_e, msg_handler_func>::iterator i = mReqMessageHandler.find(method_id);

        if( i == mReqMessageHandler.end() )
        {
            SYS_LOG(E_DEBUG, "<handle_sip_msg> Unhandled request message type %d", method_id);
            return -1;
        }
        else
        {
            method.append(pSipMsg->line.req.method.name.ptr, pSipMsg->line.req.method.name.slen);
            pSipMsg->line.req.uri->vptr->p_print(PJSIP_URI_IN_REQ_URI, pSipMsg->line.req.uri, requri, (pj_size_t)512);
            SYS_LOG(E_DEBUG, "<handle_sip_msg> %s: %s", method.c_str(), requri);            

            MAP_SYNC_B
                session = find_session(pSipMsg);
                i->second(session, pSipMsg, pBuf, length);
            MAP_SYNC_E
        }
        break;
    }
    case PJSIP_RESPONSE_MSG:
    {
        code = pSipMsg->line.status.code;
        std::map<int, msg_handler_func>::iterator i = mStatusMessageHandler.find(code);
     
        if( i == mStatusMessageHandler.end() )
        {
            SYS_LOG(E_DEBUG, "handle_sip_msg: Unhandled status message type %d", code);
            return -1; 
        }
        else
        {
            reason.append(pSipMsg->line.status.reason.ptr, pSipMsg->line.status.reason.slen);
            SYS_LOG(E_NOTICE, "Status: %d %s", code, reason.c_str());

            MAP_SYNC_B
                session = find_session(pSipMsg);
                i->second(session, pSipMsg, pBuf, length);
            MAP_SYNC_E
        }
        break;
    }
    default:
        NEVER_GET_HERE();
    }
}

namespace 
{

//////////////////////////////////////////////////////////////////////////////////////
// REQUEST HANDLERS 
/////////////////////////////////////////////////////////////////////////////////////

/* INVITE MESSAGE HANDLERS BEGIN ------------------------------------------------ **/

int invite(session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length)
{
    if( NULL == session )
        return invite1(session, pSipMsg, pBuf, length);
    else
        return reinvite(session, pSipMsg, pBuf, length);
}

int invite1(session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length)
{
    char            saddr[15],
                    daddr[15];
    unsigned short  port;
    unsigned int    res,
                    net_saddr,
                    net_daddr;
    std::string     callidhash = callid_md5hash(pSipMsg);
    session_t       newsession;
    pthread_t       me = pthread_self();
    
    assert( session == NULL ); 
    //assert( gsThreadSpecData.count(me) == 1 );

    if(net_get_ip(length, pBuf, &net_saddr, &net_daddr) && (port = get_mediaport(pSipMsg)) > 0)
    {
        net_get_ip_readable(length, pBuf, saddr, daddr);

        switch( mThreadDirection[me] )
        {
            case INCOMMING_PACKETS:
            {
                newsession.ipremote     = net_saddr;
                newsession.iplocal      = net_daddr;
                newsession.portremote   = port;
                break;
            }
            case OUTGOING_PACKETS:
            {
                newsession.ipremote     = net_daddr;
                newsession.iplocal      = net_saddr;
                newsession.portlocal    = port;
                newsession.portremote   = 0;
                break;
            }
            default:
                NEVER_GET_HERE();
        }

        newsession.status = INVITE1;
        gSessionMap.insert( std::make_pair(callidhash, newsession) );
    
        SYS_LOG(E_NOTICE, "<new_session> Host %s:%d initilized new SIP session", saddr, port);
        return 1;
    }
    
    SYS_LOG(E_ERROR, "<new_session> Unable to create new session. Either ip adresses on media port is not valid");
    return 0;
}

int reinvite(session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length)
{
    unsigned int      newDestAddr;
    const std::string connaddr = get_connaddr(pSipMsg);
    unsigned short    port     = get_mediaport(pSipMsg);
    pthread_t         me       = pthread_self();

    assert(session != NULL);

    switch(session->status)
    {
        case REINVITE:
        case ESTABLISHED:
        {
            session_t *oldsession = new session_t;
            memcpy(oldsession, session, sizeof(session_t));

            net_string2ip(connaddr.c_str(), &newDestAddr);

            switch( mThreadDirection[me] )
            {
                case INCOMMING_PACKETS:
                    session->ipremote   = newDestAddr;
                    session->portremote = port;
                    sig_reinvite.emit(oldsession, session);
                    break;
                case OUTGOING_PACKETS:
                    DO_NOTHING();
                    break;
                default:
                    NEVER_GET_HERE();
            }

            session->status = REINVITE;

            SYS_LOG(E_NOTICE, "<reinvite> Started \"REINVITE\" procedure to %s", connaddr.c_str());
            delete oldsession;
            break;
        }
        case ACK1:
        {
            SYS_LOG(E_NOTICE, "<reinvite> Sending INVITE with AUTH informations %d", session->portlocal);
            session->status = INVITE2;
            break;
        }
    default:
        NEVER_GET_HERE();
    }

    return 1;
}

/* ---------------------------------------------------------------------INVITE MESSAGE HANDLERS END  **/

int ack(session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length)
{
    assert(session != NULL);

    switch( session->status )
    {
        case AUTHREQ:
            session->status = ACK1;
            break;
        case OK1:
            session->status = ESTABLISHED;   
            sig_init.emit(session);

            SYS_LOG(E_NOTICE, "<ack> New SIP session has been ESTABLISHED port is %d", session->portremote);
            break;
        case SRVUNAVAIL:
            SYS_LOG(E_DEBUG, "<ack> IMPLEMENT ME !! Service is unavailable. Clearing session informations");
            break;
        case REINVITEOK1:
            session->status = ESTABLISHED;
            SYS_LOG(E_NOTICE, "<ack> SIP PROXY confirmed REINVITE procedure");
            break;
        default:
            NEVER_GET_HERE();
    }
    return 1;
}

int bye(session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length)
{
    std::string callidhash;
    char sIp[15], dIp[15];

    if( session == NULL )
    {
        SYS_LOG(E_WARNING, "<bye> Received BYE for untracked session");
        return -1;
    }

    callidhash = callid_md5hash(pSipMsg); 
    net_ip2string(session->ipremote, sIp);
    net_ip2string(session->iplocal,   dIp);

    sig_bye.emit(session);
    SYS_LOG(E_NOTICE, "bye: Removing session between %s:%hd and %s:%hd", 
                sIp, (session->portremote), dIp, (session->portlocal));

    session->status = BYE;
    return 1;
}

//////////////////////////////////////////////////////////////////////////////////////
// STATUS HANDLERS 
/////////////////////////////////////////////////////////////////////////////////////

int ok200 (session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length)
{
    unsigned short port;
    std::string callidhash = callid_md5hash(pSipMsg);
    pthread_t me = pthread_self();

    if( session == NULL )
    {
        SYS_LOG(E_DEBUG, "<ok200> Unknown session");
        return -1;
    }

    switch(session->status)
    {
    case INVITE1:
    case INVITE2:
        if( (port = get_mediaport(pSipMsg)) == -1 )
        {
            SYS_LOG(E_ERROR, "<ok200> Unable to parse SDP message");
            return -1;
        }

        switch( mThreadDirection[me] )
        {
            case INCOMMING_PACKETS:
                session->portremote = port;
                break;
            case OUTGOING_PACKETS:
                session->portlocal = port;
                break;
            default:
                NEVER_GET_HERE();
        }
        session->status = OK1;
        break;
    case REINVITE:
        //TODO: ewentualna zmiana portu przed odbiorce? checkit rfc
        session->status = REINVITEOK1;
        SYS_LOG(E_NOTICE, "<ok200> Confirming REINVITE to proxy");
        break;
    case BYE:
        gSessionMap.erase(callidhash);
        SYS_LOG(E_NOTICE, "<ok200> Session ended gratefully. Information cleared");
        break;
    default:
        NEVER_GET_HERE();
    }
    
    return 1;
}
int par407 (session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length)
{
    if( session == NULL )
    {
        SYS_LOG(E_WARNING, "<par407> Unknown session");
        return -1;
    }

    session->status = AUTHREQ;
    SYS_LOG(E_DEBUG, "<par407> SIP 407 Proxy Authentication Required");
    return 1;
}

int srvunavail503 (session_t *session, struct pjsip_msg *pSipMsg, const char *pBuf, int length)
{
    if( session == NULL )
    {
        SYS_LOG(E_WARNING, "<srvunavail503> Unknown session");
        return -1;
    }

    session->status = SRVUNAVAIL;
    SYS_LOG(E_DEBUG, "<srvunavail503> Service is unavailable");
    return 1;
}

} // namespace
} /* namespace inout */ } // namespace sip
