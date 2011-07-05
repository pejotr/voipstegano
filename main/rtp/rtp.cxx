
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

#include "voipsteg/rtp/rtp.hxx"
#include "voipsteg/rtp/mgr.hxx"
#ifdef SEND
#include "voipsteg/rtp/send.hxx"
namespace rtp 
{
using namespace send;
}
#else
#include "voipsteg/rtp/recv.hxx"
namespace rtp
{
using namespace receiver;
}
#endif

extern "C"
{
    #include <pthread.h>

    #include "voipsteg/logger.h"
}


namespace rtp
{

void* init(void *pParam)
{

#ifdef SEND
    thread_context_t **pThreadsCollection = &__threadsCollection;
    thread_func threadFun = job;

    SYS_LOG(E_NOTICE, "Starting RTP sender threads");
    init_threads(NULL);
    rtp::manager::init_mgr();
    queues_pooling();
#else
#ifdef RECV
    SYS_LOG(E_NOTICE, "Starting RTP receiver thread");
    rtp::manager::init_mgr();
    init_recv(NULL);
#endif
#endif

}

} /* namespace rtp */
