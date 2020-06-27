/*
 * includes/config.h
 * Copyright (C) 1999, 2000, 2001, 2002 IN2P3, CNRS
 * bbftp@in2p3.fr
 * http://doc.in2p3.fr/bbftp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */ 

/****************************************************************************

  
 
 This header contains all parameters that are user configurable
 
 config.h   v 1.8.7 2000/05/23	- Header creation
            v 1.9.3 2000/10/12	- Add FIXEDDATAPORT for d0 use
            v 2.0.0 2000/12/13  - Supress DAEMONMODE(set as option
                                  on the command line)
                                - Supress FIXEDDATAPORT (set as option
                                  on the command line)
            v 2.0.2 2000/05/07  - Move RFIO to configure
            v 2.1.0 2000/05/25  - Add PRIVATE_AUTH
			v 2.2.0 2001/10/03  - Add CERTIFICATE_AUTH

*****************************************************************************/

/*! \brief 	That is the port the server is listening on
**
**  		The default port
**		for that application is 4021 which allow the daemon to be run by
**		user. This port is also used by the client in order to connect to
**		the server
**		Default 5021
**		Starting with version 1.9.3 in case of DAEMONMODE equal to 2
**		this can be overriden on the command line 
**		Used by the server and the client.
*/
#define CONTROLPORT 5021

