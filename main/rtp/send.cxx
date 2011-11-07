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

#include <sigc++/sigc++.h>

#include "voipsteg/rtp/rtp.hxx"
#include "voipsteg/rtp/mgr.hxx"
#include "voipsteg/rtp/send.hxx"
#include "voipsteg/sip/common.hxx"
//#include "voipsteg/sip/torecv.hxx"

extern "C"
{
    #include <sys/time.h>
    #include <unistd.h>
    #include <pthread.h>

    #include "voipsteg/utils.h"
    #include "voipsteg/databank.h"
}
namespace rtp
{
using namespace send;

thread_context_t *send::__threadsCollection; 

namespace
{

    //@{
    /** State handlers functions */
    int offset_routine  (thread_context_t *ctx, int packets);
    int init_routine    (thread_context_t *ctx, int packets);
    int bit_routine     (thread_context_t *ctx, int packets);
    int delay_routine   (thread_context_t *ctx, int packets);
    int idle_routine    (thread_context_t *ctx, int packets);
    //@}

    const state_t gsStateMachine[] = {
        { OFFSET, offset_routine    },
        { INIT  , init_routine      },
        { BIT   , bit_routine       },
        { DELAY , delay_routine     },
        { IDLE  , idle_routine      },
    };


    //! Send whole bit sequence
    /*!
        This function pick a bit from databank and send whole sequence
        @param zerolen Sequence delay no. 1
        @param onelen  Sequence delay no. 2
        @return  1 When bit was sent successfully
                -1 When there is no data left to be sent
     */
    enum SEND_RES send_bit(int zerolen, int onelen, thread_context_t *context, unsigned char bit);
    
    //! Send delayed packet for time given in sleeptime
    /*!
        Suspend thread for given amount of time. If sleep time overflow given time more than
        value in delta argument the packet is not send.
        @param sleeptime Amount of time that thread will be suspended
        @param delta     Maximum oveflow
        @parma context   Calling thread context
        @return          True if packet was sent, False false in all other cases
     */ 
    bool send_delayed_packet(const struct timeval *sleepTime, int delta, thread_context_t *context);

    //! Accept packet from queue
    /*!
        Removes packet from queue and decrement number of packets still queued
        @param context Thread context
     */
    void accept_packet(thread_context_t *context);
    void switch_state(thread_context_t *context);
}

void* send::init_threads(void *pParams)
{
    char szQueueId[5] = { 0x00, };
    const vsconf_value_t *queueup = vsconf_get("queue-upnum"),
                         *queuelo = vsconf_get("queue-lonum");
    int queueCnt = queueup->value.numval - queuelo->value.numval;
    int res;
    queue_desc_t queuedesc;

    __threadsCollection = new thread_context_t[queueCnt];

    for(int i(0); i < queueCnt; ++i)
    {
        pthread_mutex_init(&(__threadsCollection[i].sync), NULL);
        pthread_mutex_init(&(__threadsCollection[i].bye ), NULL);

        sprintf(szQueueId, "%d", queuelo->value.numval + i);
        queuedesc.pNfqHandle = netfilter_init_queue(szQueueId, handle_queue);
        queuedesc.queuefd = nfq_fd(queuedesc.pNfqHandle);
        queuedesc.queueNum = queuelo->value.numval + i;

        __threadsCollection[i].queuedesc = queuedesc;
        __threadsCollection[i].packetsqueued = 0;
        __threadsCollection[i].state = OFFSET;
        __threadsCollection[i].delayCnt = 5;
        gettimeofday(&(__threadsCollection[i].sleep), NULL);

        SYS_LOG(E_DEBUG, "Binding to queue %s", szQueueId);
        __threadsCollection[i].threadid = pthread_create(&__threadsCollection[i].thread, NULL, job, 
                                                static_cast<thread_context_t*>(&__threadsCollection[i]));
    }
    SYS_LOG(E_NOTICE, "Starting rtp::init. %d queue descriptors created", queueCnt);

    return NULL;
}

int send::queues_pooling()
{
    fd_set          rfds;
    struct timeval  tv;
    int             retval, nfds = 0, rv, fd;
    char            buf[4096];

    const vsconf_value_t *queueup = vsconf_get("queue-upnum"),
                         *queuelo = vsconf_get("queue-lonum");
    int queueCnt = queueup->value.numval  - queuelo->value.numval;
    packet_entity_t packet;

    FD_ZERO(&rfds);

    for(int i(0); i < queueCnt; ++i)
    {
        FD_SET(send::__threadsCollection[i].queuedesc.queuefd, &rfds);
        if( send::__threadsCollection[i].queuedesc.queuefd > nfds ) 
        {
            nfds = send::__threadsCollection[i].queuedesc.queuefd;
        }
    }

    SYS_LOG(E_DEBUG, "Queues pooling");

    while(1)
    {
        tv.tv_sec = 2000;
        tv.tv_usec = 0;
        retval = select(nfds + 1, &rfds, NULL, NULL, &tv);
        
        if( retval )
        {
            for( int i(0); i < queueCnt; ++i)
            {
                if( FD_ISSET(send::__threadsCollection[i].queuedesc.queuefd, &rfds) )
                {
                    pthread_mutex_lock(&(send::__threadsCollection[i].sync));
                    if( (rv = recv(send::__threadsCollection[i].queuedesc.queuefd, buf, sizeof(buf), MSG_DONTWAIT)) && 
                        rv >= 0 )
                    {
                        send::__threadsCollection[i].packetsqueued += 1;
                        memcpy(packet.buffer, buf, rv);
                        packet.rv = rv;
                        send::__threadsCollection[i].packetsQueue.push_back(packet);
                        pthread_cond_signal(&send::__threadsCollection[i].startcond); 
                    } 
                    pthread_mutex_unlock(&(send::__threadsCollection[i].sync));
                }
            }
        }
        else if( retval == -1 )
        {
            SYS_LOG(E_ERROR, "select() error");
        }
        else
        {
            SYS_LOG(E_DEBUG, "No data");
        }
    }
    return 1;
}

int send::handle_queue(struct nfq_q_handle *pQh, struct nfgenmsg *pNfmsg, struct nfq_data *pNfa, void *pData)
{
    struct nfqnl_msg_packet_hdr *ph;
    int id;

    ph = nfq_get_msg_packet_hdr(pNfa);
    if(ph) {
        id = ntohl(ph->packet_id);
    }

    return nfq_set_verdict(pQh, id, NF_ACCEPT, 0, NULL);
}


void* send::job(void *params)
{
    enum SEND_RES       sendres;
    enum SEND_STATE     sendstate;
    thread_context_t    *ctx = static_cast<thread_context_t*>(params);
    const state_t       *state;
    int                 res = 1;
    int                 packetsCnt = 0;

    pthread_cond_init(&(ctx->startcond), NULL);

    while(1)
    {
        pthread_mutex_lock(&(ctx->sync));
        while( res )
        {
            SYS_LOG(E_DEBUG, "<job> Waiting for more packetes. Currently %d is queued", ctx->packetsqueued);
            pthread_cond_wait( &(ctx->startcond), &(ctx->sync));
            break;
        }
        packetsCnt = ctx->packetsqueued;
        pthread_mutex_unlock(&(ctx->sync));

        state = &(gsStateMachine[ctx->state]);
        assert(state->thisstate == ctx->state);

        res = state->handler(ctx, packetsCnt);
    } 
}

namespace {

int offset_routine  (thread_context_t *ctx, int packets)
{
    struct timeval curr, diff;

    if( packets < 1 )
    {
        SYS_LOG(E_DEBUG, "<offset_routine> Not enough packets to perform offset");
        return 1;
    }

    ctx->offsetCnt--;
    if( ctx->offsetCnt )
    {
        gettimeofday(&curr, NULL);
        diff = utils_difftimeval(&(ctx->sleep), &curr);

        if( diff.tv_sec == 0 && ( diff.tv_usec < T_20MS ) )
        {
            SYS_LOG(E_DEBUG, "<offset_routine> Sleep for %ld to be sure transmission will be ok", T_20MS - diff.tv_usec);
            usleep(T_20MS - diff.tv_usec); 
        }

        pthread_mutex_lock(&(ctx->sync));
            gettimeofday(&(ctx->sleep), NULL);
            accept_packet(ctx);
        pthread_mutex_unlock(&(ctx->sync));
        return 0;
    }
    else
    {
        SYS_LOG(E_DEBUG, "<offset_routine> Sending thread change state from OFFSET to INIT");
        ctx->state = INIT;
        return 0;
    }
}

int init_routine(thread_context_t *ctx, int packets)
{
    if( packets < 1 )
    {
        SYS_LOG(E_DEBUG, "<init_routine> Not enough packets to perform offset");
        return 1;
    }
    
    usleep(200000);

    SYS_LOG(E_DEBUG, "<init_routine> Sending thread change state from INIT to DELAY");
    ctx->state = DELAY;
    return 0;
}

int bit_routine(thread_context_t *ctx, int packets)
{
    unsigned char bit = db_pick_bit();
    unsigned int  res = 0;
    const vsconf_value_t *d0 = vsconf_get("d0"),
                         *d1 = vsconf_get("d1");

    if( packets < 3 )
    {
        SYS_LOG(E_DEBUG, "<bit_routine> Not enough packets to perform bit routine <%d>", packets);
        return 1;
    }

    if( bit == DB_NO_DATA )
    {
        SYS_LOG(E_NOTICE, "<bit_routine> TRANSFERING DATA HAS BEEN FINISHED !!!");
        ctx->state = IDLE;
        return 0;
    }

    pthread_mutex_lock(&(ctx->sync));
        res = send_bit(d0->value.numval, d1->value.numval, ctx, bit);
    pthread_mutex_unlock(&(ctx->sync));

    switch( res )
    {
        case RSP_ERROR:
            SYS_LOG(E_DEBUG, "<bit_routine> Sending bit failed, returning to DELAY state");

            ctx->state = DELAY;
            break;
        case RSP_OK:
            SYS_LOG(E_NOTICE, "<bit_routine> Sending bit succedeed, returning to DELAY state");
            rtp::manager::release_token(ctx);
            db_pop_bit();         

            ctx->state = DELAY;       
            break;
        default:
            NEVER_GET_HERE(); 
    }

    gettimeofday(&(ctx->sleep), NULL);
    return 0;
}

int delay_routine(thread_context_t *ctx, int packets)
{
    struct timeval curr, diff;

    if( packets < 1 )
    {
        SYS_LOG(E_DEBUG, "<delay_routine> Not enough packets to perform delay");
        return 1;
    }

    if( ctx->bHasToken and ctx->delayCnt == 0 )
    {
        SYS_LOG(E_DEBUG, "<delay_routine> Exiting delay routine, changing state to BIT");

        ctx->delayCnt = 5;
        ctx->state = BIT;
        return 0;
    }

    //! Variable length sleep time 
    gettimeofday(&curr, NULL);
    diff = utils_difftimeval(&(ctx->sleep), &curr);
            
    if( diff.tv_sec == 0 && diff.tv_usec < T_20MS)
    {
        usleep(T_20MS - diff.tv_usec);
    }
    //else
    //{
    //    accep_packet(ctx);
    //    continue;
    //}

    pthread_mutex_lock(&(ctx->sync));
        accept_packet(ctx);
    pthread_mutex_unlock(&(ctx->sync));

    if( !(ctx->delayCnt == 0) )
    {
        ctx->delayCnt -= 1;
    }
    gettimeofday(&(ctx->sleep), NULL);

    return 0; 
}

int idle_routine(thread_context_t *ctx, int packets)
{
    struct timeval curr, diff;

    if( packets < 1 )
    {
        SYS_LOG(E_DEBUG, "<idle_routine> Not enough packets to perform idle");
        return 1;
    }

    gettimeofday(&curr, NULL);
    diff = utils_difftimeval(&(ctx->sleep), &curr);

    if( diff.tv_sec == 0 && diff.tv_usec < T_20MS)
    {
        usleep(T_20MS - diff.tv_usec);
    }

    pthread_mutex_lock(&(ctx->sync));
        accept_packet(ctx);
    pthread_mutex_unlock(&(ctx->sync));

    if( ctx->bHasToken )
    {
        rtp::manager::release_token(ctx);
    }

    return 0;
}


}

namespace
{

enum SEND_RES send_bit(int zerolen, int onelen, thread_context_t *ctx, unsigned char bit)
{
    int     delay_0, delay_1;
    struct timeval tv, curr, diff;
    bool    res;


    if( ( bit & 0x01 ) == 0 ) 
    {
        SYS_LOG(E_DEBUG, "Sending bit <0>");
        delay_0 = zerolen;
        delay_1 = 2 * T_20MS - delay_0;
    }
    else
    {
        SYS_LOG(E_DEBUG, "Sending bit <1>");
        delay_0 = onelen;
        delay_1 = 2 * T_20MS - delay_0;
    }


    tv.tv_sec = 0;
    tv.tv_usec =  delay_0;
    res = send_delayed_packet(&tv, 1000, ctx);
    if( res == false )
    {
        return RSP_ERROR;
    }

    tv.tv_sec = 0;
    tv.tv_usec = delay_1;
    res = send_delayed_packet(&tv, 1000, ctx);
    if( res == false )
    {
        return RSP_ERROR;
    }

    return RSP_OK;
}

bool send_delayed_packet(const struct timeval *sleepTime, int delta, thread_context_t *ctx)
{
    int res;
    struct timeval start,       //< To measure real sleep time - start moment 
                   stop,        //< To measure real sleep time - stop  moment
                   diff,        //< Real sleep time
                   sleep, 
                   delayed;     //< Time thrad already slept

    sleep = *sleepTime;

    //! Trying to limit sleep time for more accurancy 
    gettimeofday(&start, NULL);
    delayed = utils_difftimeval(&(ctx->sleep), &start);

    if( delayed.tv_sec == 0 && delayed.tv_usec < sleepTime->tv_usec )
    {
        sleep.tv_usec = sleepTime->tv_usec - delayed.tv_usec;
        SYS_LOG(E_DEBUG, "<send_delayed_packet> Sleep time limited to %ld", sleep.tv_usec);
    }

    //! Sleep time measurement
    gettimeofday(&start, NULL);
        res = select(1, NULL, NULL, NULL, &sleep);

        if( res != 0 )
        {
            SYS_LOG(E_ERROR, "Select failed");
            return -1;
        }

    gettimeofday(&stop, NULL);

    diff.tv_sec  = stop.tv_sec  - start.tv_sec;
    diff.tv_usec = stop.tv_usec - start.tv_usec;
    if( diff.tv_usec < 0 )
    {
        diff.tv_usec += 1000000;
        --diff.tv_sec;
    }

//    diff = utils_difftimeval(&start, &stop);

    if(diff.tv_sec == 0 && diff.tv_usec < sleepTime->tv_usec + delta )
    {
        accept_packet(ctx);
        return true;
    }
    else
    {
        SYS_LOG(E_DEBUG, "<send_delayed_packet> Not sending a bit, thread slept for too long <%lds %ldus>", diff.tv_sec, diff.tv_usec);
        SYS_LOG(E_DEBUG, "<send_delayed_packet> Max sleep time: %ldus", sleepTime->tv_usec + delta);
        usleep(30000);
        return false;
    }

    return true;
}

void accept_packet(thread_context_t *ctx)
{
    if( ctx->packetsqueued <= 0 )
    {
        SYS_LOG(E_ERROR, "<accept_packet> No packets available to send !!!");
        return;
    }

    packet_entity_t packet = ctx->packetsQueue.front();
    nfq_handle_packet(ctx->queuedesc.pNfqHandle, packet.buffer, packet.rv);

    ctx->packetsQueue.pop_front();
    ctx->packetsqueued -= 1;
    gettimeofday(&(ctx->sleep), NULL);
}

} 
}
