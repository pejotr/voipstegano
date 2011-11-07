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

#ifndef _VOIPSTEG_RTP_SEND_
#define _VOIPSTEG_RTP_SEND_

#include "voipsteg/rtp/rtp.hxx"

extern "C"
{
    #include "voipsteg/logger.h"
}

namespace rtp
{
    namespace send 
    {

    enum SEND_STATE { OFFSET = 0, INIT, BIT, DELAY, IDLE };
    enum SEND_RES   { RSP_EMPTY, RSP_OK, RSP_ERROR };


    //! Information about packet picked from queue
    typedef struct
    {
        char buffer[4096];
        int  rv;
    } packet_entity_t;

    //! Sender thread context
    typedef struct 
    {
        //@{
        /** Thread control fields */
        pthread_t       thread;
        pthread_cond_t  /*cv,*/ startcond;
        pthread_mutex_t sync, bye;
        int             threadid;
        //@}

        //@{
        /** Packets information fields */
        queue_desc_t    queuedesc;
        int             packetsqueued;
        std::list<packet_entity_t> packetsQueue;
        //@}

        //@{
        /** Packets control fields */
        int             offsetCnt;
        //@}

        enum SEND_STATE state;
        bool            bInitTransmission;
        int             delayCnt;
        bool            bHasToken;
        struct timeval  sleep;
    } thread_context_t;

    //! State handling function type
    typedef int (*state_hnd_fun)(thread_context_t*, int);

    typedef struct
    { 
        SEND_STATE      thisstate;
        state_hnd_fun   handler;
    } state_t;

    const int   T_20MS = 20000;
    extern      thread_context_t* __threadsCollection; 

    void*   init_threads(void *pParams);
    int     queues_pooling();
    int     handle_queue(struct nfq_q_handle *pQh, struct nfgenmsg *pNfmsg, struct nfq_data *pNfa, void *pData);
    void*   job(void* pParam);

    }
}

#endif /* _VOIPSTEG_RTP_SEND_ */
