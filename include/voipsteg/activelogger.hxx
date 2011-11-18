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

#ifndef _VOIPSTEG_ACTIVELOGGER_
#define _VOIPSTEG_ACTIVELOGGER_


#include "voipsteg/logger.h"
#include <string>

namespace activelogger {

    #define APP_LOG(LEVEL, FORMAT, ...) activelogger::put((LEVEL), __LINE__, __FILE__, FORMAT, ##__VA_ARGS__);

    //! Module logger reperesentation
    typedef struct activelogger_logger {
	//std::sstream stream;
	std::string  module;
    } activelogger_t;

    //! Event information used by logger to create log message
    typedef struct activelogger_message {	
	enum LOG_LEVEL level;
	std::string    module,
	               message,
	               file;
	int            line;
	clock_t        time;
    } message_t;

    //! Initialize activelogger functionality
    /*!
        This function starts separate thread just to handle messegaes put
	put into the queue by module loggers. Because printing out infosx is 
	done in separated thread, the main routines are not weight down 
	by additional operations. Simplified implmentation of ActiveObject.
    */
    pthread_t* exe();

    //! Creates and register activelogger for submodule
    activelogger_t* create_logger(const std::string& modulename);

    //! Shut down module logger
    void destroy_logger(activelogger_t *logger);

    //! Store one line of log
    void put(LOG_LEVEL lev, int lineno, const char *file, const char *format, ...);
}


#endif /* _VOIPSTEG_ACTIVELOGGER_  */
