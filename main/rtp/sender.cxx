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
#include "voipsteg/rtp/sender.hxx"
#include "voipsteg/sip/common.hxx"
#include "voipsteg/sip/inout.hxx"

namespace rtp { namespace sender {

namespace 
{
    sender_context_t *mSenderInstances;
    unsigned int      mFreeSenderInstances;

    void inline LOCK_SENDER_INSTANCE(sender_context_t *c) { pthread_mutex_lock(c->blockMtx);   }
    void inline FREE_SENDER_INSTANCE(sender_context_t *c) { pthread_mutex_unlock(c->blockMtx); }

    //! Create new sender instance
    /*!
        Initializae all required fields, run two threads assign queue for
        packetes.
        One thread is responsible for RTP covert channel (used to send data), 
        the other one is for RTP service channel (receive ACK, NAK).
    */
    void create_new();

    //! Find free, ready to use sender instance
    int grab_sender();

    void slot_newstream(sip::session_t *session);
    void slot_delstream(sip::session_t *session);
    void slot_reinvite (sip::session_t *session);
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

    pthread_mutex_init(&mSenderInstancesMtx, NULL);

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

        mSenderInstances[i].busy             = false;
        mSenderInstances[i].covertChanQueue  = covertChanQueue;
        mSenderInstances[i].serviceChanQueue = serviceChanQueue;
    }

}

namespace
{

void create_new()
{
    int instanceIdx;

    instanceIdx = grab_free_sender(mSenderInstances);

}

sender_context_t* grab_sender()
{
    const vsconf_value_t *streamsCount = vsconf_get(RTP_STREAMS_COUNT);
    sender_context_t     *sender;

    for(int i(0); i < streamsCount->value.numval; ++i)
    {
        sender = &mSenderInstances[i];

        if(sender.busy == false)
        {
            LOCK_SENDER_INSTANCE(sender);
            
            if(sender->busy == false)
            {
                sender->busy = true;
                mFreeSenderInstances -= 1;
                FREE_SENDER_INSTANCE(sender);
                
                return sender;
            }

            FREE_SENDER_INSTANCE(sender);
        }
    }
}

void slot_newstream(sip::session_t *session)
{
    SYS_LOG(E_DEBUG, "<slot_newstream> New stream event");
    create_new();
}

void slot_delstream(sip::session_t *session)
{
    SYS_LOG(E_DEBUG, "<slot_delstream> New stream event");
}

void slot_reinvite(sip::session_t *session)
{
    SYS_LOG(E_DEBUG, "<slot_reivinte> New stream event");
}

}

} /* namespace sender */ } /* namespace rtp  */
