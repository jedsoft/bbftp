/*
 * includes/common.h
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

  
 
 This header contains all parameters that help the readibility of software,
 either in client or server, or parameters that are common to client and
 server.
 
 common.h v 1.8.7 2000/05/23  - Header creation

*****************************************************************************/

/* COMPRESSION and NOCOMPRESSION :
**		Often used in routine call (more readable than 0 or 1)
*/
#define COMPRESSION 	1
#define NOCOMPRESSION 	0

/* ACKTO :
**		Define the time out to wait for an acknoledge. An acknoledge is sent
**		by a child to the corresponding child to say that all data have been
**		received.
**		Expressed in second, usual value 100.
*/
#define ACKTO	100

/* CONTROLSOCKTO :
**		Define the time out to wait while reading on the control socket.
**		This time out is set only if the server or the client knows that 
**		the other side  has something to send 
**		Expressed in second, usual value 180.
*/
#define CONTROLSOCKTO	180

/* SENDCONTROLTO :
**		Define the time out to wait while sending on the control socket.
**		Expressed in second, usual value 180.
*/
#define SENDCONTROLTO	180

/* DATASOCKTO :
**		Define the time out to wait while reading on the data socket.
**		This time out is set only if the server or the client knows that 
**		the other side  has something to send 
**		Expressed in second, usual value 300.
*/
#define DATASOCKTO	300

/* CHECKSTDINTO :
**      Define the time to wait for the connection on the checkreceive
**      socket by the server
**		Expressed in second, usual value 300.
*/
#define CHECKSTDINTO    300

/* NBITSINKEY :
**		Define the number of bits in the RSA Key
*/
#define NBITSINKEY	1024

/* TCPWINSIZE :
**		Define the TCP window size for the data socket.
**		Expressed in bytes, usual value is 128000 for OSF1 (more needs priviledge)
**		and 256000 for the others OS.
*/
#ifdef OSF1
#define TCPWINSIZE	128000
#else
#define TCPWINSIZE	256000
#endif

/* UTILS
 */
#define FREE(X) if(X != NULL) { free(X), X=NULL;}
