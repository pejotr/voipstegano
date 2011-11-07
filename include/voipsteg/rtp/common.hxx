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

#ifndef _VOIPSTEG_RTP_COMMON_
#define _VOIPSTEG_RTP_COMMON_

#include "voipsteg/sip/commmon.hxx"

namespace rtp 
{
    //! Common function for RTP modules
    namespace common {

        //! Reaction to reinvite procedure
        void reinvite(sip::session_t *session);

    } /* namespace common */
} /* namespace rtp */


#endif /* _VOIPSTEG_RTP_COMMON_ */
