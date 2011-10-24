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

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include <map>
#include <list>

#include "voipsteg/activelogger.hxx"

using namespace activelogger;

namespace activelogger {

namespace {

//! Stores already created module  loggers
std::map<std::string, activelogger_t*> mCreatedLoggers;

//! Messages awaiting to be printed
std::list<message_t> mMessageQueue;

//@{
/** Message queue synchronization  */
pthread_t       mThread;
pthread_mutex_t mQueueSync;
pthread_cond_t  mQueueNotEmpty;
//@}

volatile bool   mRun;

//! Main logger loop
/*!
    Runs in own thread.
    Pick messages from queue and write them to output. When there is no 
    message awaiting, thread execution is suspended.
 */
void* main_loop(void* pParams);

} // namespace

pthread_t* init(void* pParams)
{
    int res;

    pthread_mutex_init(&mQueueSync, NULL);
    res = pthread_create(&mThread, NULL, main_loop, NULL);

    if( res )
    {
	fprintf(stderr, " ------------ FATAL ERROR ------------\n");
	fprintf(stderr, " Unable to initialize logger subsystem\n");
	fprintf(stderr, " -------------------------------------\n");
	exit(-1);
    }

    return &mThread;
}


activelogger_t* create_logger(const std::string& modulename)
{
    std::map<std::string, activelogger_t*>::iterator i;
    activelogger_t* logger;

    i = mCreatedLoggers.find(modulename);

    if(i == mCreatedLoggers.end()) {
	logger = new activelogger_t;
	logger->module = modulename;

	mCreatedLoggers[modulename] = logger;
	return logger;
    }

    return i->second;
}

void put(LOG_LEVEL lev, int lineno, const char *file, const char *format, ...)
{
    char      log[512] = {0x00, };
    message_t message;
    va_list   args;

    va_start (args, format);
    vsprintf(log, format, args);

    message.message = log;
    message.file    = file;
    message.line    = lineno;
    message.time    = clock();

    pthread_mutex_lock(&mQueueSync);
        mMessageQueue.push_back(message);
    pthread_mutex_unlock(&mQueueSync);
}

namespace {

    void* main_loop(void* pParams)
    {
	message_t msg;
	double milis;

	while(mRun) 
	{
	    pthread_mutex_lock(&mQueueSync);

	    if( mMessageQueue.size() == 0  )
	    {
		pthread_cond_wait(&mQueueNotEmpty, &mQueueSync);
	    }

	    msg = mMessageQueue.front();
	    mMessageQueue.pop_front();

	    pthread_mutex_unlock(&mQueueSync);

	    milis = ((double)msg.time / (double)CLOCKS_PER_SEC)*1000;
	    vslog_log(msg.level, msg.line, msg.file.c_str(), milis, msg.message.c_str());
      	}
    }
}

} // activelogger
