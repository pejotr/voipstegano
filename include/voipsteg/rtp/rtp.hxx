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

#ifndef _VOIPSTEG_RTP_
#define _VOIPSTEG_RTP_

#include <list>

extern "C" 
{
    #include <unistd.h>
    #include <time.h>
    
    #include "voipsteg/config.h"
    #include "voipsteg/netfilter.h"
}

namespace rtp 
{


typedef struct 
{
    struct nfq_handle   *pNfqHandle;
    int                 queuefd;
    int                 queueNum;
} queue_desc_t;


typedef void*   (*thread_func)(void*);
typedef int     (*msg_handler_func)(struct pjsip_msg *pSipMsg, const char *pBuf, int length);
typedef int     (*handle_func)(struct nfq_q_handle *pQh, struct nfgenmsg *pNfmsg, struct nfq_data *pNfa, void *pData);

void* init(void *pParam);

}

#endif /* _VOIPSTEG_RTP_ */
