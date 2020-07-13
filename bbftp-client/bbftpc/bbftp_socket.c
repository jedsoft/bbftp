/*
 * bbftpd/bbftp_socket.c
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
 
 bbftp_socket.c    v 3.0.0 2003/02/24  - Routines creation
              
*****************************************************************************/
#include <bbftp.h>

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include "_bbftp.h"

#include <common.h>
#include <client.h>
#include <client_proto.h>
#include <structures.h>

/*******************************************************************************
** bbftp_createdatasock :                                                      *
**                                                                             *
**      Routine to create a receive socket                                     *
**      This routine may send MSG_INFO                                         *
**                                                                             *
**      INPUT variable :                                                       *
**           portnumber   :  portnumber to connect    NOT MODIFIED             *
**                                                                             *
**      OUPUT variable :                                                       *
**          logmessage :  to write the error message in case of error          *
**                                                                             *
**      RETURN:                                                                *
**          -1   Creation failed unrecoverable error                           *
**           0   OK                                                            *
**           >0  Creation failed recoverable error                             *
**                                                                             *
*******************************************************************************/
int bbftp_createdatasock(int portnumber/*,char *logmessage*/) 
{
    int    sock ;
    struct sockaddr_in data_source ;
    int    on = 1 ;
    int tcpwinsize ;
#if defined(SUNOS) || defined(_HPUX_SOURCE) || defined(IRIX)
    int        addrlen ;
#else
    socklen_t        addrlen ;
#endif
    int    retcode ;

    /* Declaring the variables need to change the value of the TOS */
    int tos_value, tos_len;
    int qbss_value = 0x20;
    tos_len = sizeof(tos_value);
    (void) tos_len;

    sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ;
    if ( sock < 0 ) {
        printmessage(stderr,CASE_ERROR,91,BBftp_Timestamp,"Cannot create data socket: %s\n",strerror(errno));
        return (-1) ;
    }
    if ( setsockopt(sock,SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)) < 0 ) {
        close(sock) ;
        if (BBftp_Warning) printmessage(stderr,CASE_ERROR,92,BBftp_Timestamp,"Cannot set SO_REUSEADDR on data socket : %s\n",strerror(errno));
        return (-1) ;
    }
    tcpwinsize = 1024 * BBftp_Recvwinsize ;
    if ( setsockopt(sock,SOL_SOCKET, SO_RCVBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)) < 0 ) {
        /*
        ** In this case we are going to log a message
        */
        if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,23,BBftp_Timestamp,"Cannot set SO_RCVBUF on data socket : %s\n",strerror(errno));
    }
    addrlen = sizeof(tcpwinsize) ;
    if ( getsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char *)&tcpwinsize,&addrlen) < 0 ) {
        close(sock) ;
        printmessage(stderr,CASE_ERROR,93,BBftp_Timestamp,"Cannot get SO_RCVBUF on data socket : %s\n",strerror(errno));
        return (-1) ;
    }
    if ( tcpwinsize < 1024 * BBftp_Recvwinsize ) {
        if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,24,BBftp_Timestamp,"Receive buffer on port %d cannot be set to %d Bytes, Value is %d Bytes",portnumber,1024*BBftp_Recvwinsize,tcpwinsize) ;
    }
    tcpwinsize = 1024 * BBftp_Sendwinsize ;
    if ( setsockopt(sock,SOL_SOCKET, SO_SNDBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)) < 0 ) {
        /*
        ** In this case we just log an error
        */
        if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,25,BBftp_Timestamp,"Cannot set SO_SNDBUF on data socket : %s\n",strerror(errno));
    }
    addrlen = sizeof(tcpwinsize) ;
    if ( getsockopt(sock,SOL_SOCKET, SO_SNDBUF,(char *)&tcpwinsize,&addrlen) < 0 ) {
        close(sock) ;
        printmessage(stderr,CASE_ERROR,93,BBftp_Timestamp,"Cannot get SO_SNDBUF on data socket : %s\n",strerror(errno));
        return (-1) ;
    }
    if ( tcpwinsize < 1024 * BBftp_Sendwinsize ) {
        if ( BBftp_Warning ) printmessage(stderr,CASE_WARNING,26,BBftp_Timestamp ,"Send buffer on port %d cannot be set to %d Bytes, Value is %d Bytes",portnumber,1024*BBftp_Sendwinsize,tcpwinsize) ;
    }
    if ( setsockopt(sock,IPPROTO_TCP, TCP_NODELAY,(char *)&on,sizeof(on)) < 0 ) {
        close(sock) ;
        printmessage(stderr,CASE_ERROR,94,BBftp_Timestamp,"Cannot set TCP_NODELAY on data socket : %s\n",strerror(errno));
        return (-1) ;
    }
	if ( (BBftp_Transferoption & TROPT_QBSS ) == TROPT_QBSS ) {
		/* Setting the value for IP_TOS to be 0x20 */
		if ( setsockopt(sock,IPPROTO_IP, IP_TOS, (char *)&qbss_value, sizeof(qbss_value)) < 0 ) {
		    close(sock) ;
			printmessage(stderr,CASE_ERROR,95,BBftp_Timestamp, "Cannot set IP_TOS on data socket : %s\n", strerror(errno));
			return (-1) ;
		}
	}

    data_source.sin_family = AF_INET;
/*
#ifdef SUNOS
    data_source.sin_addr = BBftp_My_Ctladdr.sin_addr;
#else
    data_source.sin_addr.s_addr = INADDR_ANY;
#endif
    data_source.sin_port = 0;
    if ( bind(sock, (struct sockaddr *) &data_source,sizeof(data_source)) < 0) {
        close(sock) ;
        printmessage(stderr,CASE_ERROR,95,BBftp_Timestamp,"Cannot bind on data socket on port %d: %s\n",portnumber,strerror(errno));
        return (-1) ;
    }
*/
    data_source.sin_addr = BBftp_His_Ctladdr.sin_addr;
    data_source.sin_port = htons(portnumber);
    addrlen = sizeof(data_source) ;
    retcode = connect(sock,(struct sockaddr *) &data_source,addrlen) ;
    if ( retcode < 0 ) {
        close(sock) ;
        printmessage(stderr,CASE_ERROR,96,BBftp_Timestamp,"Cannot connect to data socket on port %d: %s\n",portnumber,strerror(errno));
        if ( errno == EINTR || errno == ETIMEDOUT ) {
            /*
            ** In this case we are going to tell the calling program 
            ** to retry so we close the socket and set the return code
            ** to zero 
            */
            return 0 ;
        } else {
            if ( errno == EPERM || errno == EACCES) {
                printmessage(stderr,CASE_ERROR,96,BBftp_Timestamp,"This is probably caused by a firewall rule on the server");
	    }
            return (-1) ;
        }
    }
	if ( BBftp_Debug ) printmessage(stdout,CASE_NORMAL,91,BBftp_Timestamp,"Connected to port: %d\n",portnumber);
    return sock ;
}
