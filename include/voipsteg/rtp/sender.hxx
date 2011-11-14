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

        //! Covert channel possible states
        namespace SENDER_COVERT_STATE
        {
            enum e { 
                INIT,    /* initialization                                 */
                BITSEND, /* seding bit state                               */
                ACKWAIT, /* sender waits for ACK to arrive on service chan */
                NOP      /* normal operation                               */
                INT,     /* sender was interrupted, SIP session ended      */
                DIE      /* session is over, covert channel ready to reuse */
            };
        }

        //! Service channel possible states
        namespace SENDER_SERVICE_STATE
        {
            enum e {
                INIT,    /* initialization                                 */
                HALFACK, /* first possib;e packet of ACK                   */
                ACK,     /* covert trasmission ACK                         */    
                DIE      /* service channel ready to reuse                 */
            };
        }

        //! Internal data for controlling covert channel
        typedef rtp_sender_covert_nfo {
            std::list<packet_wrapper_t> packets; /* queued packets      */
            queue_desc_t covertChanQueue;        /* channel queue info  */
            SENDER_COVERT_STATE::e state;        /* current state       */
        } sender_covert_nfo_t;

        //! Internal data for controlling service channel
        typedef rtp_sender_service_nfo {
            queue_desc_t serviceChanQueue;       /* channel queue info  */
        } sender_service_nfo_t ;
 
        //! Sender instance description and private data
        typedef struct rtp_sender_context 
        {      
            //@{
            /** Threads control */
            pthread_t covertChanThrd;
            pthread_t serviceChanThrd;

            //! Sync concurrent operations on sender instance 
            pthread_mutex_t blockMtx;

            //! Sender status indicators
            bool busy;          /*  */
            bool alive;
            bool interrupt;
            //@}

            //@{
            /** Private submodules data */
            sender_covert_nfo_t covertNfo;
            sender_service_nfo_t serviceNfo;
            //@}
        } sender_context_t;

        //! Initialize sender module
        void initialize();

    } /* namespace sender  */
} /* namespace rtp  */



#endif /* _VOIPSTEG_RTP_SENDER_ */
