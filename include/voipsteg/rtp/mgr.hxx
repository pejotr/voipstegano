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

#ifndef _VOIPSTEG_RTP_MGR_
#define _VOIPSTEG_RTP_MGR_

#include "voipsteg/rtp/rtp.hxx"
#include "voipsteg/rtp/send.hxx"
#include "voipsteg/sip/common.hxx"

namespace rtp
{
    namespace manager
    {

    enum QUEUE_STATUS { QS_FREE, QS_INUSE, QS_HASTOKEN };

    typedef struct {
        queue_desc_t queueDesc;
        struct timeval lastTime;
        enum QUEUE_STATUS status;
        void* data;
    } queue_info_t;

    //! Gather all required resources and start management thread
    int init_mgr();

    //! Informs that token can be taken
    /*!
        Called by sending thread that hold token. Informs manager
        that token can bit was sent and token can be assigned to
        other stream.
        @param context Sending thread context
     */
    void release_token(rtp::send::thread_context_t *context);

    //! Creates new netfilter rule based on internal session desc 
    netfilter_rule_t create_rule(sip::session_t *session);
 
    //@{
    /** Slots for controlling sending threads */
    void slot_newstream(sip::session_t *session);
    void slot_delstream(sip::session_t *session);
    void slot_reinvite (sip::session_t *oldsession, sip::session_t *session);
    //@}
    
    }
}



#endif /* _VOIPSTEG_RTP_RECV_ */
