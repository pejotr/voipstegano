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


#ifndef _VOIPSTEG_RTP_RECV_
#define _VOIPSTEG_RTP_RECV_

#include "voipsteg/rtp/rtp.hxx"

extern "C"
{
    #include "voipsteg/logger.h"
}


namespace rtp
{
    namespace receiver 
    {

    enum VALIDATION { BIT_1, BIT_2 };

    typedef struct 
    {
        struct timeval delayFromPrevious; //!< How much time did passed after last packet from this queue
        int seqStep;    //!< Step num in sequence
        int history;    //!< What bit we are looking for

        enum VALIDATION state; 
        char bit_1, bit_2;

        queue_desc_t desc;
    } recv_context_t;


    //extern thread_context_t *__threadsCollection; 

    void* init_recv(void *pParams);
    }
}


#endif /* _VOIPSTEG_RTP_RECV_ */
