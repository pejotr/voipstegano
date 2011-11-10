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

#ifndef _VOIPSTEG_RTP_SENDER_
#define _VOIPSTEG_RTP_SENDER_

#include <list>

#include "voipsteg/netfilter.h"
#include "voipsteg/net.h"

namespace rtp
{
    //! All logic for sending data using covert RTP channel
    namespace sender
    {

        //! Sender possible states
        enum SENDER_STATE { 
            BITSEND, /* seding bit state   */
            ACKWAIT  /* sender waits for ACK to arrive on service chan */
        };

        //! Sender instance description and private data
        typedef struct rtp_sender_context 
        {      
            //@{
            /** Threads control */
            pthread_t covertChanThrd;
            pthread_t serviceChanThrd;

            //! Hold thread in idle state until session is assigned
            pthread_cond_t startCond;

            //! Blocks concurrent operations on sender instance 
            pthread_mutex_t blockMtx;

            //! Sender status indicators
            bool busy;
            bool alive;
            //@}

            //! Queued packets
            std::list<packet_wrapper_t> packets;

            //! Current state
            SENDER_STATE state;

            //@{
            /** Queues and packets */
            queue_desc_t covertChanQueue;
            queue_desc_t serviceChanQueue;
            //@}
        } sender_context_t;
    
        //! Initialize sender module
        void initialize();

    } /* namespace sender  */
} /* namespace rtp  */



#endif /* _VOIPSTEG_RTP_SENDER_ */
