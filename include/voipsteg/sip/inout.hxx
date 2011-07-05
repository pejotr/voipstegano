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

#ifndef _VOIPSTEG_SIP_INOUT_
#define _VOIPSTEG_SIP_INOUT_

#include <map>
#include <sigc++/sigc++.h>

extern "C" {
    #include <netinet/in.h>
    #include <linux/netfilter.h>
    #include <libnetfilter_queue/libnetfilter_queue.h>

    #include <pjsip.h>
    #include <pjlib-util.h>
    #include <pjlib.h>

    #include "voipsteg/net.h"
    #include "voipsteg/netfilter.h"

    #include "md5/md5.h"
}


namespace sip 
{
    typedef struct sip_common_session session_t;

    namespace inout 
    {   
    //@{
    /** Signals */
    extern sigc::signal<void, session_t*> sig_init;
    extern sigc::signal<void, session_t*> sig_bye;
    extern sigc::signal<void, session_t*, session_t*> sig_reinvite;
    //@}

    //! Initialize SIP capturing system for incomming packets
    void* init(void *pParam);

    //! Queue handling function
    /*!
        Pick packets from SIP in queue and analyze them looking for RTP port
        and IP address of sender and receiver
    */
    int handle_queue(struct nfq_q_handle *pQh, struct nfgenmsg *pNfmsg, struct nfq_data *pNfa, void *pData);
        
    //! Handle outgoing SIP messages
    /*!
        Check if received message is important for appllication. If it is
        a appropiate function is called
    */
    int handle_sip_msg(struct pjsip_msg* pSipMsg, const char *pBuf, int length);
    }
}

#endif /* _VOIPSTEG_SIP_TORECV_ */

