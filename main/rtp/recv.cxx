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

#include "voipsteg/rtp/recv.hxx"
#include "voipsteg/rtp/rtp.hxx"

extern "C"
{
    #include "voipsteg/databank.h"    

    
    #include <assert.h>
    #include <sys/types.h>
    #include <sys/socket.h>
}

namespace rtp
{

static int __indexOffset;
receiver::recv_context_t *__queueInfo;

namespace 
{
    using namespace receiver;
    pthread_mutex_t sync;
    struct timeval mem;

    pthread_mutex_t gsSync;
    int gsDelta, gsD0, gsD1;

    int gsHistory, gsSeqStep;
    struct timeval gsMem, gsDiff;
    enum VALIDATION gsState;
    char gsBit1, gsBit2;

    int check0();
    int check1();
    int check_delays();
    int handle_queue(struct nfq_q_handle *pQh, struct nfgenmsg *pNfmsg, struct nfq_data *pNfa, void *pData);
    int validate_bit(int bit);

}

void* receiver::init_recv(void *pParams)
{
    const struct timeval defaulttime = { 200, 0 };
    int     rv, fd, nfds = 0;
    char    buf[4096],
            szQueueId[6] = { 0x00, };
    queue_desc_t    queuedesc;
    recv_context_t  context;
    fd_set          rfds;
    struct timeval  tv;
    int             retval;
    int             sec, 
                    usec,
                    bit,
                    ena;

    const vsconf_value_t *queueup = vsconf_get("queue-upnum"),
                         *queuelo = vsconf_get("queue-lonum"),
                         *delta   = vsconf_get("delta"),
                         *d0      = vsconf_get("d0"),
                         *d1      = vsconf_get("d1");
    int queueCnt = queueup->value.numval  - queuelo->value.numval;

    ena = 1;

    SYS_LOG(E_NOTICE, "Receiver main routine");

    __indexOffset = queuelo->value.numval;
    __queueInfo = new recv_context_t[queueCnt];

    gsDelta = delta->value.numval;
    gsD0    = d0->value.numval;
    gsD1    = d1->value.numval;

    SYS_LOG(E_NOTICE, "Receiving parameters: <D0[%d]; D1[%d]; Delta[%d]>", gsD0, gsD1, gsDelta);

    gsSeqStep = 0;
    gsHistory = -1;
    gsSeqStep = BIT_1;

    FD_ZERO(&rfds);
    for(int i(0); i < queueCnt; ++i)
    {
        sprintf(szQueueId, "%d", queuelo->value.numval + i);
        queuedesc.pNfqHandle = netfilter_init_queue(szQueueId, handle_queue);
        queuedesc.queuefd    = nfq_fd(queuedesc.pNfqHandle);

        if( setsockopt(queuedesc.queuefd, SOL_SOCKET, SO_TIMESTAMP, &ena, sizeof(ena)) )
        {
            SYS_LOG(E_WARNING, "Unable to enable timestamping");
        }

        context.desc = queuedesc;
        context.delayFromPrevious.tv_sec = 0;
        context.delayFromPrevious.tv_usec = 0;
        context.seqStep = 0;
        context.history = -1;
        context.state = BIT_1;

        __queueInfo[i] = context;

        FD_SET(queuedesc.queuefd, &rfds);
        if( queuedesc.queuefd > nfds )
        {
            nfds = queuedesc.queuefd;
        }

        SYS_LOG(E_NOTICE, "Binding to queue %d successful", queuelo->value.numval + i);
    }

    while(1)
    {
        tv.tv_sec  = 200;
        tv.tv_usec = 0;

        retval = select(nfds + 1, &rfds, NULL, NULL, &tv);

        if( retval == -1)
        {
            SYS_LOG(E_ERROR, "Select error");
            return NULL;
        }
        else if( retval )
        {
            /*
            sec = 200 - tv.tv_sec;
            usec = 0 - tv.tv_usec;

            if( usec < 0 )
            {
                sec--;
                usec += 1000000;
            }
            */

            for( int i(0); i < queueCnt; ++i )
            {
                //__queueInfo[i].delayFromPrevious.tv_sec  += sec;
                //__queueInfo[i].delayFromPrevious.tv_usec += usec;

                if( FD_ISSET(__queueInfo[i].desc.queuefd, &rfds)) 
                {
                    rv = recv(__queueInfo[i].desc.queuefd, buf, sizeof(buf), 0);
                    nfq_handle_packet(__queueInfo[i].desc.pNfqHandle, buf, rv);

                    bit = check_delays();
                    validate_bit(bit);

                    //__queueInfo[i].delayFromPrevious.tv_sec  = 0;
                    //__queueInfo[i].delayFromPrevious.tv_usec = 0;
                }
            }

        }
        /*
        else
        {
            for( int i(0); i < queueCnt; ++i )
            {
                __queueInfo[i].delayFromPrevious.tv_sec  += 200;
                __queueInfo[i].delayFromPrevious.tv_usec += 0;
            }
           
            SYS_LOG(E_DEBUG, "Timeout");
        }
        */
    }
}



namespace
{


int handle_queue(struct nfq_q_handle *pQh, struct nfgenmsg *pNfmsg, struct nfq_data *pNfa, void *pData)
{
    struct nfqnl_msg_packet_hdr *ph;
    struct timeval tv, diff;
    int id;
    
    ph = nfq_get_msg_packet_hdr(pNfa);
    if(ph) 
    {
        id = ntohl(ph->packet_id);
    }

    if(!nfq_get_timestamp(pNfa, &tv))
    {
        diff.tv_sec = tv.tv_sec - gsMem.tv_sec;
        diff.tv_usec = tv.tv_usec - gsMem.tv_usec;

        if( diff.tv_usec < 0 )
        {
            diff.tv_usec += 1000000;
            --diff.tv_sec;
        }
    
        gsDiff  = diff;
        gsMem   = tv;
    }

    return nfq_set_verdict(pQh, id, NF_ACCEPT, 0, NULL);
}


#define FIRST_SYMBOL -1
#define RESET        -3

int check_delays()
{
    //SYS_LOG(E_DEBUG, " >>>> DELAY %ld", gsDiff.tv_usec);

    if(gsHistory == -1)
    {
        int ret1 = check1();
        int ret2 = check0();

        if( ret1 == 1 )
        {
            //SYS_LOG(E_DEBUG, "<1> BIT");
            gsHistory = 1;
            gsSeqStep = 1;
            return FIRST_SYMBOL;
        }
        else if(ret2 == 1)
        {
            //SYS_LOG(E_DEBUG, "<0> BIT");
            gsHistory = 0;
            gsSeqStep = 1;
            return FIRST_SYMBOL;
        }
        else
        {
            gsHistory = -1;
            gsSeqStep = 0;
            return RESET;
        }
    }
    else
    {
        switch(gsHistory)
        {
        case 0:
            if( check0() == 2 )
            {
                //db_put_bit(0x00);
                return 0;
            }
            break;
        case 1:
            if( check1() == 2 )
            {
                //db_put_bit(0x01);
                return 1;
            }
            break;
        default:
            NEVER_GET_HERE();
        }
       
        return RESET;
    }

    NEVER_GET_HERE();
    return RESET;
}

int check0()
{
    const int delta = 3500;

    switch(gsSeqStep)    
    {
        case 0: 
            if( (gsDiff.tv_usec > 10000 - gsDelta) && 
                (gsDiff.tv_usec < 10000 + gsDelta) )
            {
                return 1;
            }
            else
            {
                return 0;
            }
            break;
        case 1:
            assert(gsHistory == 0);
            if( (gsDiff.tv_usec > 30000 - gsDelta) && 
                (gsDiff.tv_usec < 30000 + gsDelta) )
            {
                gsSeqStep =  0;
                gsHistory = -1;
                SYS_LOG(E_NOTICE, "Poprawna sekwencja - 0");
                return 2;
            }
            else
            {
                SYS_LOG(E_DEBUG, "Wygladalo ok, ale nie - 0");
                gsSeqStep =  0;
                gsHistory = -1;
                return -2;
            }
            break;
        default:
            NEVER_GET_HERE();
    }

}

int check1()
{
    switch(gsSeqStep)    
    {
        case 0:
            if( (gsDiff.tv_usec > 30000 - gsDelta) && 
                (gsDiff.tv_usec < 30000 + gsDelta) )
            {
                return 1;
            }
            else
            {
                return 0;
            }
            break;
        case 1:
            assert(gsHistory == 1);
            if( (gsDiff.tv_usec > 10000 - gsDelta) && 
                (gsDiff.tv_usec < 10000 + gsDelta) )
            {
                gsSeqStep =  0;
                gsHistory = -1;
                SYS_LOG(E_NOTICE, "Poprawna sekwencja - 1");
                return 2;
            }
            else
            {
                gsSeqStep =  0;
                gsHistory = -1;
                SYS_LOG(E_DEBUG, "Wygladalo ok, ale nie - 1");
                return -2;
            }
            break;
        default:
            NEVER_GET_HERE();
    }
}

int validate_bit(int bit)
{
    if( bit < 0 )
        return 0;

    switch( gsState )
    {
        case BIT_1:
            SYS_LOG(E_DEBUG, "Storing bit value");
            gsBit1 = bit & 0x01;
            gsState = BIT_2;
            break;
        case BIT_2:
            gsBit2 = bit & 0x01;
            SYS_LOG(E_DEBUG, "bit1 = %d , bit2 = %d", gsBit1, gsBit2);
            if( !( gsBit1 ^ gsBit2) )
            {
                db_put_bit(gsBit1 & 0x01);
                gsState = BIT_1;
            }
            else
            {
                SYS_LOG(E_WARNING, "Transmission corrupted");
                gsState = BIT_2;
                gsBit1 = gsBit2;
                if( gsBit2 == 0)
                {
                    SYS_LOG(E_WARNING, "Propably bit 1");
                    db_put_bit(0x01);
                }
                else
                {
                    SYS_LOG(E_WARNING, "Propably bit 0");
                    db_put_bit(0x00);
                }
            }
            break;
        default:
            NEVER_GET_HERE();
    }

    return 1;

}

}
}
