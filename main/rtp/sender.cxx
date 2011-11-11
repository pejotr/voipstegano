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
    typedef struct rtp_state_machine {
        SENDER_COVERT_STATE::e   state;
        state_action_t handler;
    } state_t;

    //! State handlers
    int state_bitsend(sender_context_t *ctx);
    int state_ackwait(sender_context_t *ctx);
    int state_init   (sender_context_t *ctx);
    int state_die    (sender_context_t *ctx);

    //! Machine definition
    /*!
        States in array HAVE TO appear in the same order as they
        are defined in enum SENDER_COVERT_STATE
        \see SENDER_COVERT_STATE
        \see send_controller
     */
    state_t mSenderFSM[] = {
        { SENDER_COVERT_STATE::INIT   , state_init    },
        { SENDER_COVERT_STATE::BITSEND, state_bitsend },
        { SENDER_COVERT_STATE::ACKWAIT, state_ackwait },
        { SENDER_COVERT_STATE::INT    , state_int     },
        { SENDER_COVERT_STATE::DIE    , state_die     }
    };

    //@}


    sender_context_t *mSenderInstances;
    unsigned int      mFreeSenderInstances;

    void inline LOCK_SENDER_INSTANCE(sender_context_t *c) { pthread_mutex_lock(&c->blockMtx);   }
    void inline FREE_SENDER_INSTANCE(sender_context_t *c) { pthread_mutex_unlock(&c->blockMtx); }

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
     */
    void* send_controller(void *pParams);

    //! Sender routine for service channel 
    void* recv_controller(void *pParams);


    //! Find free, ready to use sender instance
    sender_context_t* grab_sender();

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
            netfilter_init_queue(covertFrom->value.numval + i,handle_queue);
        covertChanQueue.queuefd = nfq_fd(covertChanQueue.pNfqHandle);
        covertChanQueue.queueNum = covertFrom->value.numval + i;

        serviceChanQueue.pNfqHandle = 
            netfilter_init_queue(serviceFrom->value.numval + i,handle_queue);
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
    state_t *state;
    packet_wrapper_t packet;
    sender_context_t *senderCtx = static_cast<sender_context_t*>(pParams);
    int nfds = senderCtx->covertChanQueue.queuefd;

    FD_ZERO(&rfds);
    FD_SET(senderCtx->covertChanQueue.queuefd, &rfds);

    while(1)
    {
        tv.tv_sec = 2000;
        tv.tv_usec = 0;

        ret = select(nfds + 1, &rfds, NULL, NULL, &tv);

        if(ret)
        {
            if(FD_ISSET(senderCtx->covertChanQueue.queuefd, &rfds))
            {
                // TODO: Czy synchronizacja jest potrzebna?
                //       Czy istnieje inne watki ktore moge chciec modyfikwoac
                //       dane watku nadawczego ?

                //LOCK_SENDER_INSTANCE(senderCtx);
                if((rv = recv(senderCtx->covertChanQueue.queuefd, buf, sizeof(buf), MSG_DONTWAIT)) &&
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

                state = &mSenderFSM[senderCtx->state];
                state->handler(senderCtx);
            }
        }
        else if(ret == -1)
        {
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

void* recv_controller(void *pParams)
{
    while(1)
    {
    }
}


////////////////////////////////////////////////////////////////////////////////
// STATE HANDLERS 
////////////////////////////////////////////////////////////////////////////////

int state_bitsend(sender_context_t *ctx)
{
    return 1;
}

_______________________________________________________________________________
int state_ackwait(sender_context_t *ctx)
{
    return 1;
}

_______________________________________________________________________________
int state_init(sender_context_t *ctx)
{
    return 1;
}

_______________________________________________________________________________
int state_int(sender_context_t *ctx)
{
    return 1;
}

_______________________________________________________________________________
int state_die(sender_context_t *ctx)
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
        // setup initial state
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
            LOCK_SENDER_INSTANCE(sender);
            
                if(sender->busy == false && 
                    sender->covertNfo.state == DIE)
                {
                    sender->busy = true;
                    freeSender = sender;
                    mFreeSenderInstances -= 1;
                }

            FREE_SENDER_INSTANCE(sender);
        }
    }

    return freeSender;
}


}

} /* namespace sender */ } /* namespace rtp  */
