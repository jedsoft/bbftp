/*
 * bbftpc/getdatasock.c
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

  
  
 getdatasock.c  v 0.0.0 2000/01/11
                v 1.6.0 2000/03/24  - Set Receive buffer to TCPWINSIZE
                                      for the get option
                v 1.7.1 2000/04/04  - Variable cleaning (detected on IRIX)
                v 1.8.7 2000/05/28  - Change headers
                v 1.8.9 2000/08/08  - Add timestamp
                v 1.9.0 2000/08/21  - Use configure to help portage
                v 2.0.0 2000/03/19  - Use protocol version 2
                v 2.0.1 2001/04/19  - Correct indentation 
                                    - Port to IRIX

*****************************************************************************/
#include <bbftp.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include "_bbftp.h"

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <structures.h>

int getdatasock(int nbsock, int *errcode) 
{
    int     i ;
    int     tcpwinsize ;
    int     on = 1 ;
#if defined(SUNOS) || defined(_HPUX_SOURCE) || defined(IRIX)
    int     addrlen ;
#else
    socklen_t  addrlen ;
#endif
    struct  linger li ;
    struct  sockaddr_in sck ;
    int     *mysockfree ;
    int     *myportfree;
    int     port ;
    
    /* Declaring the variables need to change the value of the TOS */
    int tos_value, tos_len;
    int qbss_value = 0x20;
    tos_len = sizeof(tos_value);
    (void) tos_len;

    mysockfree = mysockets ;
    myportfree = myports ;
    sck = myctladdr ;
    for (i=0 ; i<nbsock ; i++) {
        sck.sin_port = 0;
        *mysockfree = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ;
        if ( *mysockfree < 0 ) {
            printmessage(stderr,CASE_ERROR,91,timestamp,"Cannot create data socket: %s\n",strerror(errno));
            *errcode = 91 ;
            return -1 ;
        }
        if ( setsockopt(*mysockfree,SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)) < 0 ) {
            printmessage(stderr,CASE_ERROR,92,timestamp,"Cannot set SO_REUSEADDR on data socket : %s\n",strerror(errno));
            *errcode = 92 ;
            return -1 ;
        }
        if ( setsockopt(*mysockfree,IPPROTO_TCP, TCP_NODELAY,(char *)&on,sizeof(on)) < 0 ) {
            printmessage(stderr,CASE_ERROR,93,timestamp,"Cannot set TCP_NODELAY on data socket : : %s\n",strerror(errno));
            *errcode = 93 ;
            return -1 ;
        }

		/* Printing out the default value for the TOS 
			if( getsockopt(*mysockfree,IPPROTO_IP,IP_TOS, 
		       &tos_value,
		       &tos_len) == 0){
			   }
		*/
		if ( (transferoption & TROPT_QBSS ) == TROPT_QBSS ) {
			/* Setting the value for IP_TOS to be 0x20 */
			if ( setsockopt(*mysockfree,IPPROTO_IP, IP_TOS, (char *)&qbss_value, sizeof(qbss_value)) < 0 ) {
				printmessage(stderr,CASE_ERROR,93,timestamp, "Cannot set IP_TOS on data socket : : %s\n", strerror(errno)); 
			}
		}

        /*
        ** Send and receive are inversed because they are supposed
        ** to be the daemon char
        */
        tcpwinsize = 1024 * recvwinsize ;
        if ( setsockopt(*mysockfree,SOL_SOCKET, SO_SNDBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)) < 0 ) {
            addrlen = sizeof(tcpwinsize) ;
            if ( getsockopt(*mysockfree,SOL_SOCKET,SO_SNDBUF,(char *)&tcpwinsize,&addrlen) < 0 ) {
                if ( warning) printmessage(stderr,CASE_WARNING,23,timestamp,"Unable to get send buffer size : %s\n",tcpwinsize,strerror(errno));
            } else {
                if ( warning) printmessage(stderr,CASE_WARNING,24,timestamp,"Send buffer cannot be set to %d Bytes, Value is %d Bytes\n",1024*recvwinsize,tcpwinsize);
            }
        }
        tcpwinsize = 1024 * sendwinsize ;
        if ( setsockopt(*mysockfree,SOL_SOCKET, SO_RCVBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)) < 0 ) {
            addrlen = sizeof(tcpwinsize) ;
            if ( getsockopt(*mysockfree,SOL_SOCKET,SO_RCVBUF,(char *)&tcpwinsize,&addrlen) < 0 ) {
                if ( warning) printmessage(stderr,CASE_WARNING,25,timestamp,"Unable to get receive buffer size : %s\n",tcpwinsize,strerror(errno));
            } else {
                if ( warning) printmessage(stderr,CASE_WARNING,26,timestamp,"Receive buffer cannot be set to %d Bytes, Value is %d Bytes\n",1024*sendwinsize,tcpwinsize);
            }
        }    
        li.l_onoff = 1 ;
        li.l_linger = 1 ;
        if ( setsockopt(*mysockfree,SOL_SOCKET,SO_LINGER,(char *)&li,sizeof(li)) < 0 ) {
            printmessage(stderr,CASE_ERROR,94,timestamp,"Cannot set SO_LINGER on data socket : %s\n",strerror(errno));
            *errcode = 94 ;
            return -1 ;
        }
        if (pasvport_min) { /* Try to bind within a range */
                for (port=pasvport_min; port<=pasvport_max; port++) {
                sck.sin_port = htons(port);
                    if (bind(*mysockfree, (struct sockaddr *) &sck, sizeof(sck)) >= 0) break;
                }
                if (port>pasvport_max) {
                printmessage(stderr,CASE_ERROR,95,timestamp,"Cannot bind on data socket : %s\n",strerror(errno));
                *errcode = 95 ;
                return -1 ;
                }
        } else {
            if ( bind(*mysockfree, (struct sockaddr *) &sck ,sizeof(sck))  < 0) {
                printmessage(stderr,CASE_ERROR,95,timestamp,"Cannot bind on data socket : %s\n",strerror(errno));
                *errcode = 95 ;
               return -1 ;
            }
        }
        addrlen = sizeof(sck) ;
        if (getsockname(*mysockfree,(struct sockaddr *)&sck, &addrlen) < 0) {
            printmessage(stderr,CASE_ERROR,96,timestamp,"Cannot getsockname on data socket : %s\n",strerror(errno));
            *errcode = 96 ;
            return -1 ;
           }
        if (listen(*mysockfree, 1) < 0) {
		  printmessage(stderr,CASE_ERROR,97,timestamp,"Cannot listen on data socket : %s\n",strerror(errno));
            *errcode = 97 ;
            return -1 ;
        }
        *myportfree++ =  sck.sin_port ;
        if ( debug ) printmessage(stdout,CASE_NORMAL,0,timestamp,"Listen on port %d\n", ntohs(sck.sin_port)) ;
        mysockfree++ ;
    }
    return 0 ;
}
