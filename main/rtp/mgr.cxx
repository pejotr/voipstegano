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

extern "C" {
    #include <time.h>
    #include <sys/time.h>
    #include <limits.h>
    #include <errno.h>

    #include "voipsteg/netfilter.h"
}

#include <sigc++/sigc++.h>
#include <pthread.h>

#include "voipsteg/rtp/mgr.hxx"
#include "voipsteg/rtp/send.hxx"
#include "voipsteg/sip/inout.hxx"
#include "voipsteg/sip/common.hxx"

using namespace rtp::manager;

static queue_info_t     *gsQueues;
static pthread_t        gsThread;
static pthread_mutex_t  gsSyncMtx, gsTokenMtx;
static pthread_cond_t   gsTokenCv;

namespace
{
    //! Main thread loop - run in separate thread
    void* manager_loop(void*);

    //! Decide which stream should be used to send a bit
    /*!
        @return Index in gsQueues collection
     */
    int choose_stream();

    //! Wait for action or until time expire
    int timeout_wait();

    //! Check if there is free queue available to use
    /*!
        Scan queues info collection in order to find free queue.
        @return Index in gsQueues
     */
    int grab_queue(); 
    int release_queue(int queueIdx);

    //@{
    /** Token control functions */
    void grant_token(int streamIdx);

    //! When timeout occured token is taken from thread
    void take_token(int streamIdx);
    //@}
}

//! Gather all required resources and start management thread
int rtp::manager::init_mgr()
{
    #ifdef SEND 
        const vsconf_value_t *queueup = vsconf_get("queue-upnum"),
                             *queuelo = vsconf_get("queue-lonum");
        int queueCnt = queueup->value.numval - queuelo->value.numval;
        struct timeval curr;

        gettimeofday(&curr, NULL);
        pthread_mutex_init(&gsTokenMtx, NULL);

        sip::inout::sig_init.connect    ( sigc::ptr_fun(slot_newstream) );
        sip::inout::sig_reinvite.connect( sigc::ptr_fun(slot_reinvite)  );
        sip::inout::sig_bye.connect     ( sigc::ptr_fun(slot_delstream) );

        gsQueues = new queue_info_t[queueCnt];
        if( gsQueues != NULL )
        {
            for(int i(0); i < queueCnt; ++i)
            {
                gsQueues[i].lastTime = curr;
                gsQueues[i].status = QS_FREE;
                gsQueues[i].data = static_cast<void*>(&(rtp::send::__threadsCollection[i]));
            }

            pthread_create(&gsThread, NULL, manager_loop, NULL);
            return 1;
        }
        return -1;
    #else
    #ifdef RECV
        const vsconf_value_t *queueup = vsconf_get("queue-upnum"),
                         *queuelo = vsconf_get("queue-lonum");
        int queueCnt = queueup->value.numval - queuelo->value.numval;

        pthread_mutex_init(&gsTokenMtx, NULL);
        pthread_mutex_init(&gsSyncMtx, NULL);

        sip::inout::sig_init.connect    ( sigc::ptr_fun(slot_newstream) );
        sip::inout::sig_reinvite.connect( sigc::ptr_fun(slot_reinvite)  );
        sip::inout::sig_bye.connect     ( sigc::ptr_fun(slot_delstream) );

        gsQueues = new queue_info_t[queueCnt];
        if( gsQueues != NULL )
        {
            for(int i(0); i < queueCnt; ++i)
            {
                gsQueues[i].status = QS_FREE;
            }
            return 1;
        }
    #endif
    #endif

    NEVER_GET_HERE();
    
}

void rtp::manager::release_token(rtp::send::thread_context_t *context)
{
    SYS_LOG(E_DEBUG, "Token returned");
    context->bHasToken = false;
    pthread_cond_signal(&gsTokenCv);
    return;
}

netfilter_rule_t rtp::manager::create_rule(sip::session_t *session)
{
    char szLocIp[16] = { 0x00, }, 
         szRemIp[16] = { 0x00, };
    netfilter_rule_t rule; 

    net_ip2string(session->ipremote, szRemIp);
    net_ip2string(session->iplocal  , szLocIp);

    #ifdef SEND 
        sprintf(rule.szSrcPort, "%d", session->portlocal);
        sprintf(rule.szDstPort, "%d", session->portremote);
        sprintf(rule.szSrcIp,   "%s", szLocIp);
        sprintf(rule.szDstIp,   "%s", szRemIp);
    #else
    #ifdef RECV
        sprintf(rule.szSrcPort, "%d", session->portremote);
        sprintf(rule.szDstPort, "%d", session->portlocal  );
        sprintf(rule.szSrcIp,   "%s", szRemIp);
        sprintf(rule.szDstIp,   "%s", szLocIp);
    #endif
    #endif

    sprintf(rule.szQueue,   "%d", session->queueNum  );
    strcpy(rule.szChain,    session->szChain);
    return rule;
}

void rtp::manager::slot_newstream(sip::session_t *session)
{
    int queueIdx;
    netfilter_rule_t rule;
    const vsconf_value_t *offset = vsconf_get("offset");

    SYS_LOG(E_DEBUG, "<slot_newstream> New stream event");
    queueIdx = grab_queue();
    if( queueIdx != -1 )
    {
        #ifdef SEND
            rtp::send::thread_context_t *ctx;

            ctx = static_cast<rtp::send::thread_context_t*>(gsQueues[queueIdx].data);
            session->queueNum = ctx->queuedesc.queueNum;
            strcpy(session->szChain, NF_CHAIN_OUTPUT);

            pthread_mutex_lock(&(ctx->sync));
            ctx->bInitTransmission = true;
            ctx->offsetCnt = offset->value.numval; 
            ctx->state = rtp::send::OFFSET;
            pthread_mutex_unlock(&(ctx->sync));

            rule = create_rule(session);
            netfilter_manage_rule(&rule, ADD);
            SYS_LOG(E_DEBUG, "<slot_newstream> New rule has been added to IPTABLES");
            return;
        #else
        #ifdef RECV
            const vsconf_value_t *queueup = vsconf_get("queue-upnum"),
                                 *queuelo = vsconf_get("queue-lonum");

            session->queueNum = queueIdx + queuelo->value.numval;
            strcpy(session->szChain, NF_CHAIN_INPUT);

            rule = create_rule(session);
            netfilter_manage_rule(&rule, ADD);
            SYS_LOG(E_DEBUG, "<slot_newstream> New rule has been added to IPTABLES");
            return;
        #endif
        #endif

        NEVER_GET_HERE();
    }
    
    SYS_LOG(E_DEBUG, "<slot_newstrem> No free queue");
}

void rtp::manager::slot_delstream(sip::session_t *session)
{
    #ifdef SEND
        const vsconf_value_t *queueup = vsconf_get("queue-upnum"),
                             *queuelo = vsconf_get("queue-lonum");
        int queueCnt                = queueup->value.numval - queuelo->value.numval;
        int streamIdx = session->queueNum - queuelo->value.numval;
        rtp::send::thread_context_t* ctx = 
                    static_cast<rtp::send::thread_context_t*>(gsQueues[streamIdx].data);

        SYS_LOG(E_DEBUG, "<slot_delstream> Clearing queue %d", session->queueNum);

        pthread_mutex_lock(&(ctx->bye));
        pthread_mutex_lock(&(ctx->sync));
            ctx->packetsqueued = 0;
            ctx->packetsQueue.clear();        
            release_queue(streamIdx);
            SYS_LOG(E_DEBUG, "<slot_delstream> Queue cleared", session->queueNum);
        pthread_mutex_unlock(&(ctx->sync));
        pthread_mutex_unlock(&(ctx->bye));
        return;
    #else
    #ifdef RECV
        const vsconf_value_t *queueup = vsconf_get("queue-upnum"),
                             *queuelo = vsconf_get("queue-lonum");

        SYS_LOG(E_DEBUG, "<slot_delstream> Removing binding <queueNum[%d]>", session->queueNum);

        int streamIdx = session->queueNum - queuelo->value.numval;
        release_queue(streamIdx);
        return;
    #endif
    #endif

    NEVER_GET_HERE();
    return;
}

void rtp::manager::slot_reinvite (sip::session_t *oldsession, sip::session_t *session)
{
    netfilter_rule_t    oldrule, newrule;

    if( sip::cmp_sessions(oldsession, session) )
    {
        SYS_LOG(E_DEBUG, "<slot_reinvite> Skipping reinvite event because session remains the same");
        return;
    }
    
    oldrule = create_rule(oldsession),
    newrule = create_rule(session);

    SYS_LOG(E_DEBUG, "<slot_reinvite> Modyfying iptables rules due to reinvite procedure");
    netfilter_manage_rule(&newrule, ADD);
    netfilter_manage_rule(&oldrule, DEL);
}

namespace
{

void* manager_loop(void*)
{
    const vsconf_value_t *streamchoose = vsconf_get("streamchoose");
    int streamIdx, res;
    unsigned long sleepUSec = 150000; // TODO: Config file ?

    while(true)
    {
        streamIdx = choose_stream();

        // Conditional variable? streamsCnt > 0 ?
        if( streamIdx >= 0 )
        {
            SYS_LOG(E_DEBUG, "Next stream to use %d", streamIdx);
            grant_token(streamIdx);
            res = timeout_wait();

            switch(res)
            {
                case ETIMEDOUT:
                    SYS_LOG(E_NOTICE, "Stream was holding token for too long. Force to take token back");
                case 0:
                    take_token(streamIdx);
                    break;
                case EINVAL:
                    SYS_LOG(E_ERROR, "The value specified by abstime is invalid");
                    break;
                default:
                    NEVER_GET_HERE();
            }
        }

        usleep(streamchoose->value.numval);
    }

}

int choose_stream()
{
    const vsconf_value_t *queueup = vsconf_get("queue-upnum"),
                         *queuelo = vsconf_get("queue-lonum");
    int queueCnt = queueup->value.numval - queuelo->value.numval;
    int streamIdx = -1;
    struct timeval flagTime;

    flagTime.tv_sec  = LONG_MAX;
    flagTime.tv_usec = 0;

    for(int i(0); i < queueCnt; ++i)
    {
        if( gsQueues[i].status == QS_INUSE && timercmp(&(gsQueues[i].lastTime), &flagTime, <) )
        {
            flagTime = gsQueues[i].lastTime;
            streamIdx = i;
        }        
    }

    return streamIdx;
}

int timeout_wait()
{
    const vsconf_value_t *tokenswitch = vsconf_get("tokenswitch");
    int             res;
    struct timespec ts;

    pthread_mutex_lock(&gsTokenMtx);
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += tokenswitch->value.numval;
    res = pthread_cond_timedwait(&gsTokenCv, &gsTokenMtx, &ts);
    pthread_mutex_unlock(&gsTokenMtx);

    return res;
}

int grab_queue()
{
    const vsconf_value_t *queueup = vsconf_get("queue-upnum"),
                         *queuelo = vsconf_get("queue-lonum");
    int queueCnt = queueup->value.numval - queuelo->value.numval;
    
    pthread_mutex_lock(&gsSyncMtx);
    for(int i(0); i < queueCnt; ++i)
    {
        if(gsQueues[i].status == QS_FREE)
        {
            gsQueues[i].status = QS_INUSE;
            pthread_mutex_unlock(&gsSyncMtx);
            SYS_LOG(E_DEBUG, "Assigned queue num is %d", queuelo->value.numval + i);
            return i;
        }
    }

    SYS_LOG(E_DEBUG, "There is no free queue left");
    pthread_mutex_unlock(&gsSyncMtx);
    return -1;
}

int release_queue(int queueIdx)
{
    pthread_mutex_lock(&gsSyncMtx);
            gsQueues[queueIdx].status = QS_FREE;
            SYS_LOG(E_DEBUG, "<release_queue> Queue %d released", queueIdx);
    pthread_mutex_unlock(&gsSyncMtx);
    return 1;
}

void grant_token(int streamIdx)
{
    rtp::send::thread_context_t *ctx = static_cast<rtp::send::thread_context_t*>(gsQueues[streamIdx].data);
    ctx->bHasToken = true;
}

void take_token(int streamIdx)
{
    rtp::send::thread_context_t *ctx = static_cast<rtp::send::thread_context_t*>(gsQueues[streamIdx].data);
    ctx->bHasToken = false;
}

}
