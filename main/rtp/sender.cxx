/* vim: set expandtab tabstop=4 softtabstop=4 shiftwidth=4:  */
/*
 * Copyright (C) 2011  Piotr Doniec, Warsaw University of Technology
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

#include <assert.h>
#include <sigc++/sigc++.h>

#include "voipsteg/config.h"
#include "voipsteg/netfilter.h"
#include "voipsteg/net.h"
#include "voipsteg/rtp/sender.hxx"
#include "voipsteg/sip/common.hxx"
#include "voipsteg/sip/inout.hxx"
#include "voipsteg/activelogger.hxx"

namespace rtp { namespace sender {

namespace 
{

    //@{
    /** Sender state machine desciption  */

    //! State handler function type
    typedef int (*state_action_t)(sender_context_t *ctx);

    //! State definition
    typedef struct rtp_covert_state {
        SENDER_COVERT_STATE::e state;
        state_action_t         handler;
    } covert_state_t;

    //! State definiotn for service channel
    typedef struct rtp_service_state {
        SENDER_SERVICE_STATE::e state;
    } service_state_t;

    //! State handlers
    int covert_state_bitsend(sender_context_t *ctx);
    int covert_state_ackwait(sender_context_t *ctx);
    int covert_state_init   (sender_context_t *ctx);
    int covert_state_die    (sender_context_t *ctx);
    int covert_state_nop    (sender_context_t *ctx);

    //! Machine definition
    /*!
        States in array HAVE TO appear in the same order as they
        are defined in enum SENDER_COVERT_STATE
        \see SENDER_COVERT_STATE
        \see send_controller
     */
    covert_state_t mSenderCovertFSM[] = {
        { SENDER_COVERT_STATE::INIT   , covert_state_init    },
        { SENDER_COVERT_STATE::BITSEND, covert_state_bitsend },
        { SENDER_COVERT_STATE::ACKWAIT, covert_state_ackwait },
        { SENDER_COVERT_STATE::NOP    , covert_state_nop     },
        { SENDER_COVERT_STATE::INT    , covert_state_int     },
        { SENDER_COVERT_STATE::DIE    , covert_state_die     }
    };

    service_state_t mSenderServiceFSM[] = {
    };

    //@}


    sender_context_t *mSenderInstances;
    unsigned int      mFreeSenderInstances;

    void inline LOCK_COVERT_INSTANCE(sender_context_t *c) { pthread_mutex_lock(&c->covertNfo.blockMtx);   }
    void inline FREE_COVERT_INSTANCE(sender_context_t *c) { pthread_mutex_unlock(&c->covertNfo.blockMtx); }

    //! Create new sender instance
    /*!
        Initialize all required fields, run two threads assign queue for
        packetes.
        One thread is responsible for RTP covert channel (used to send data), 
        the other one is for RTP service channel (receive ACK, NAK).
    */
    void new_stream(sip:session_t *s);

    //! Sender routine for data channel
    /*!
        Controll thread live. When this function ends, sender instance
        cannot be used - alive flag is set to false.

        \see sender_context_t
        \see recv_controller
     */
    void* send_controller(void *pParams);

    //! Sender routine for service channel 
    void* recv_controller(void *pParams);

    //! Find free, ready to use sender instance
    sender_context_t* grab_sender();
    
    //! Find RTP sender context basing on sip session
    sender_context_t* find_sender(sip::session *sip);

    //@{
    //! Operations made just before packet is accepted
    void handle_covert_packet(sender_context_t *ctx);
    void handle_service_packet(sender_context_t *ctx);
    //@}
    
    //! Accept packet
    int accept_packet(struct nfq_q_handle *pQh, struct nfgenmsg *pNfmsg, struct nfq_data *pNfa, void *pData);

    //@{
    /**   */ 
    void slot_newstream(sip::session_t *session);
    void slot_delstream(sip::session_t *session);
    void slot_reinvite (sip::session_t *session);
    //@}
}

void initialize()
{
    const vsconf_value_t *covertFrom = vsconf_get(RTP_COVERT_FROM),
        *serviceFrom = vsconf_get(RTP_SERVICE_FROM),
        *streamsCount = vsconf_get(RTP_STREAMS_COUNT);
    queue_desc_t covertChanQueue,
        serviceChanQueue;

    mSenderInstances = new sender_context_t[streamsCount->value.numval];
    mFreeSenderInstances = streamsCount->value.numval;

    for(int i(0); i < streamsCount->value.numval; ++i)
    {
        covertChanQueue.pNfqHandle = 
            netfilter_init_queue(covertFrom->value.numval + i,accept_packet);
        covertChanQueue.queuefd = nfq_fd(covertChanQueue.pNfqHandle);
        covertChanQueue.queueNum = covertFrom->value.numval + i;

        serviceChanQueue.pNfqHandle = 
            netfilter_init_queue(serviceFrom->value.numval + i,accept_packet);
        serviceChanQueue.queuefd = nfq_fd(serviceChanQueue.pNfqHandle);
        serviceChanQueue.queueNum = serviceFrom->value.numval + i;

        pthread_mutex_init(&mSenderInstances[i].blockMtx, NULL);

        mSenderInstances[i].alive            = true;
        mSenderInstances[i].busy             = false;
        mSenderInstances[i].interrupt        = false;
        mSenderInstances[i].covertChanQueue  = covertChanQueue;
        mSenderInstances[i].serviceChanQueue = serviceChanQueue;
    }

    sip::inout::sig_init.connect( sigc::ptr_fun(slot_newstream) );
}

namespace
{

void* send_controller(void *pParams)
{
    fd_set rfds;
    struct timeval tv;
    int ret, rv;
    char buf[4096];
    covert_state_t *state;
    packet_wrapper_t packet;
    sender_context_t *senderCtx = static_cast<sender_context_t*>(pParams);
    sender_covert_nfo_t *covertNfo = &senderCtx->covertNfo;
    int nfds = senderCtx->covertNfo.covertChanQueue.queuefd;

    FD_ZERO(&rfds);
    FD_SET(covertNfo.covertChanQueue.queuefd, &rfds);

    while(1)
    {
        tv.tv_sec = 2000;
        tv.tv_usec = 0;

        ret = select(nfds + 1, &rfds, NULL, NULL, &tv);

        if(ret)
        {
            if(FD_ISSET(covertNfo.covertChanQueue.queuefd, &rfds))
            {
                // TODO: Czy synchronizacja jest potrzebna?
                //       Czy istnieje inne watki ktore moge chciec modyfikwoac
                //       dane watku nadawczego ?

                //LOCK_SENDER_INSTANCE(senderCtx);
                if((rv = recv(covertNfo.covertChanQueue.queuefd, buf, sizeof(buf), MSG_DONTWAIT)) &&
                    rv >= 0)
                {
                    memcpy(packet.buf, buf, rv);
                    packet.rv = rv;
                    senderCtx->packets.push_back(packet);
                }
                //FREE_SENDER_INSTANCE(senderCtx);

                if(senderCtx->interrupt)
                {
                    APP_LOG("<send_cotroller> interrupted");
                    senderCtx->state = SENDER_COVERT_STATE::INT;
                }

                state = &mSenderCovertFSM[senderCtx->state];
                state->handler(senderCtx);
            }
        }
        else if(ret == -1)
        {
            //TODO: oproznic kolejki, zmienic stan na DIE 
            APP_LOG(E_ERROR, "<send_controller> select() error");
            senderCtx->alive = false;
            break;
        }
        else
        {
            APP_LOG(E_DEBUG, "<send_controller> no data");
        }
    }
}

________________________________________________________________________________
void* recv_controller(void *pParams)
{
    fd_set rfds;
    struct timeval tv;
    int ret, rv;
    service_state_t *state;
    sender_context_t *senderCtx = static_cast<sender_context_t*>(pParams);
    sender_service_nfo_t *serviceNfo = senderCtx->serviceNfo;
    int nfds = serviceNfo.serviceChanQueue.queuefd;

    while(1)
    {
        tv.tv_sec = 2000;
        tv.tv_usec = 0;

        ret = select(nfds + 1, &rfds, NULL, NULL, &tv);

        if(ret)
        {
            if(FD_ISSET(serviceNfo.serviceChanQueue.queuefd, &rfds))
            {
            }
        }
        else if(ret == -1)
        {
            //TODO: oproznic kolejki, zmienic stan na DIE 
            APP_LOG(E_ERROR, "<recv_controller> select() error");
            senderCtx->alive = false;
            break;
        }
        else
        {
            APP_LOG(E_DEBUG, "<recv_controller> no data");
        }

    }
}


////////////////////////////////////////////////////////////////////////////////
// STATE HANDLERS 
////////////////////////////////////////////////////////////////////////////////

int covert_state_bitsend(sender_context_t *ctx)
{
    APP_LOG(E_DEBUG, "<covert_chan> state: BITSEND");
    handle_covert_packet(ctx);
    return 1;
}

_______________________________________________________________________________
int covert_state_ackwait(sender_context_t *ctx)
{
    APP_LOG(E_DEBUG, "<covert_chan> state: ACKWAIT");
    return 1;
}

_______________________________________________________________________________
int covert_state_init(sender_context_t *ctx)
{
    APP_LOG(E_DEBUG, "<covert_chan> state: INIT");
    return 1;
}

_______________________________________________________________________________
int covert_state_int(sender_context_t *ctx)
{
    APP_LOG(E_DEBUG, "<covert_chan> state: INT");

    ctx->covertNfo.packets.clear();
    ctx->covertNfo.state = SENDER_COVERT_STATE::DIE;

    return 1;
}

_______________________________________________________________________________
int covert_state_nop(sender_context_t *ctx)
{
    APP_LOG(E_DEBUG, "<covert_chan> state: NOP");
    return 1;
}

_______________________________________________________________________________
int covert_state_die(sender_context_t *ctx)
{
    ctx->covertNfo.packets.clear();
    ctx->covertNfo.state = SENDER_COVERT_STATE::DIE;
    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// SIGNAL HANDLERS 
////////////////////////////////////////////////////////////////////////////////

void slot_newstream(sip::session_t *session)
{
    APP_LOG(E_DEBUG, "<slot_newstream> new stream event");
    new_stream(session);
}

_______________________________________________________________________________
void slot_delstream(sip::session_t *session)
{
    APP_LOG(E_DEBUG, "<slot_delstream> del stream event");
    del_stream(session);
}

_______________________________________________________________________________
void slot_reinvite(sip::session_t *session)
{
    APP_LOG(E_DEBUG, "<slot_reivinte> reinvite event");
}

////////////////////////////////////////////////////////////////////////////////
// HELPERS 
////////////////////////////////////////////////////////////////////////////////

void new_stream(sip::session *session)
{
    sender_context_t *freeSenderCtx;

    freeSenderCtx = grab_sender();

    if(freeSenderCtx)
    {
        freeSenderCtx->covert.packets.clear();
        freeSenderCtx->covert.state = SENDER_COVERT_STATE::INIT;
    }
    else
    {
        APP_LOG(E_NOTICE, "<create_new> no free queue to handle stream");
        return;
    }
}

_______________________________________________________________________________
void del_stream(sip::session *session)
{
   sender_context_t *sender = find_sender(session);
   sender->interrupt = true;
}

_______________________________________________________________________________
sender_context_t* find_sender(sip::session *sip)
{
    const vsconf_value_t *streamsCount = vsconf_get(RTP_STREAMS_COUNT);
    bool res = false;
    sender_context_t *sender = NULL;

    for(int i(0); i < streamsCount->value.numval; ++i)
    {
        if(sip::cmp_sessions(session, mSenderInstances[i].sipSession))
        {
            return &mSenderInstances[i];
        }
    }
} 

_______________________________________________________________________________
sender_context_t* grab_sender()
{
    const vsconf_value_t *streamsCount = vsconf_get(RTP_STREAMS_COUNT);
    sender_context_t *sender = NULL,
        *freeSender = NULL;

    for(int i(0); i < streamsCount->value.numval, freeSender != NULL; ++i)
    {
        if(mSenderInstances[i].busy == false)
        {        
            sender = &mSenderInstances[i];
            LOCK_COVERT_INSTANCE(sender);
            
                if(sender->busy == false && 
                    sender->covertNfo.state == DIE)
                {
                    sender->busy = true;
                    freeSender = sender;
                    mFreeSenderInstances -= 1;
                }

            FREE_COVERT_INSTANCE(sender);
        }
    }

    return freeSender;
}

_______________________________________________________________________________
void handle_covert_packet(sender_context_t *ctx)
{
    assert(ctx->covertNfo.packets.size() != 0);

    packet_wrapper_t p = ctx->covertNfo.packets.front();

    nfq_handle_packet(ctx->covertNfo.covertChanQueue.pNfqHandle, 
        packet.buffer, packet.rv);
    ctx->covertNfo.packets.pop_front();
}

_______________________________________________________________________________
void handle_service_packet(sender_context_t *ctx)
{
}

_______________________________________________________________________________
int accept_packet(struct nfq_q_handle *pQh, struct nfgenmsg *pNfmsg, 
    struct nfq_data *pNfa, void *pData);
{
    struct nfqnl_msg_packet_hdr *ph;
    int id;

    ph = nfq_get_msg_packet_hdr(pNfa);
    if(ph) {
        id = ntohl(ph->packet_id);
    }

    return nfq_set_verdict(pQh, id, NF_ACCEPT, 0, NULL);
}

}

} /* namespace sender */ } /* namespace rtp  */
