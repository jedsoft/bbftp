/*
 * bbftpd/bbftpd_socket.c
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

 
 bbftpd_socket.c    v 2.0.0 2000/12/19  - Routines creation
                    v 2.0.1 2001/04/23  - Correct indentation
                                        - Port to IRIX
               
*****************************************************************************/
#include <bbftpd.h>

#include <errno.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>

#include <netinet/tcp.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <arpa/inet.h>
#include <netinet/in.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <structures.h>

extern int  sendwinsize ;        
extern int  recvwinsize ;        
extern struct sockaddr_in his_addr;        /* Remote adresse */
extern struct sockaddr_in ctrl_addr;    /* Local adresse */
extern int  fixeddataport ;
extern int     *myports ;
extern int     *mysockets ;
extern int  transferoption ;
extern int  pasvport_min ;
extern int  pasvport_max ;

/*******************************************************************************
** bbftpd_createreceivesocket :                                                       *
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
int bbftpd_createreceivesocket(int portnumber,char *logmessage) 
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

	/*syslog(BBFTPD_INFO,"New socket to: %s:%d\n",inet_ntoa(his_addr.sin_addr), portnumber);*/
    sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ;
    if ( sock < 0 ) {
        syslog(BBFTPD_ERR, "Cannot create receive socket on port %d : %s",portnumber,strerror(errno));
        sprintf(logmessage,"Cannot create receive socket on port %d : %s",portnumber,strerror(errno)) ;
        return (-1) ;
    }
    if ( setsockopt(sock,SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)) < 0 ) {
        close(sock) ;
        syslog(BBFTPD_ERR,"Cannot set SO_REUSEADDR on receive socket port %d : %s",portnumber,strerror(errno)) ;
        sprintf(logmessage,"Cannot set SO_REUSEADDR on receive socket port %d : %s",portnumber,strerror(errno)) ;
        return (-1) ;
    }
    tcpwinsize = 1024 * recvwinsize ;
    if ( setsockopt(sock,SOL_SOCKET, SO_RCVBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)) < 0 ) {
        /*
        ** In this case we are going to log a message
        */
        syslog(BBFTPD_ERR,"Cannot set SO_RCVBUF on receive socket port %d : %s",portnumber,strerror(errno)) ;
    }
    addrlen = sizeof(tcpwinsize) ;
    if ( getsockopt(sock,SOL_SOCKET,SO_RCVBUF,(char *)&tcpwinsize,&addrlen) < 0 ) {
         syslog(BBFTPD_ERR,"Cannot get SO_RCVBUF on receive socket port %d : %s",portnumber,strerror(errno)) ;
        close(sock) ;
        sprintf(logmessage,"Cannot get SO_RCVBUF on receive socket port %d : %s",portnumber,strerror(errno)) ;
        return (-1) ;
    }
    if ( tcpwinsize < 1024 * recvwinsize ) {
        sprintf(logmessage,"Receive buffer on port %d cannot be set to %d Bytes, Value is %d Bytes",portnumber,1024*recvwinsize,tcpwinsize) ;
        reply(MSG_INFO,logmessage) ;
    }
    tcpwinsize = 1024 * sendwinsize ;
    if ( setsockopt(sock,SOL_SOCKET, SO_SNDBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)) < 0 ) {
        /*
        ** In this case we just log an error
        */
        syslog(BBFTPD_ERR,"Cannot set SO_SNDBUF  on receive socket port %d : %s",portnumber,strerror(errno)) ;
    }
    addrlen = sizeof(tcpwinsize) ;
    if ( getsockopt(sock,SOL_SOCKET, SO_SNDBUF,(char *)&tcpwinsize,&addrlen) < 0 ) {
        syslog(BBFTPD_ERR,"Cannot get SO_SNDBUF on receive socket port %d : %s",portnumber,strerror(errno)) ;
        close(sock) ;
        sprintf(logmessage,"Cannot get SO_SNDBUF on receive socket port %d : %s",portnumber,strerror(errno)) ;
        return (-1) ;
    }
    if ( tcpwinsize < 1024 * sendwinsize ) {
        sprintf(logmessage,"Send buffer on port %d cannot be set to %d Bytes, Value is %d Bytes",portnumber,1024*sendwinsize,tcpwinsize) ;
        reply(MSG_INFO,logmessage) ;
    }
    if ( setsockopt(sock,IPPROTO_TCP, TCP_NODELAY,(char *)&on,sizeof(on)) < 0 ) {
        close(sock) ;
        syslog(BBFTPD_ERR,"Cannot set TCP_NODELAY on receive socket port %d : %s",portnumber,strerror(errno)) ;
        sprintf(logmessage,"Cannot set TCP_NODELAY on receive socket port %d : %s",portnumber,strerror(errno)) ;
        return (-1) ;
    }
    data_source.sin_family = AF_INET;
#ifdef SUNOS
    data_source.sin_addr = ctrl_addr.sin_addr;
#else
    data_source.sin_addr.s_addr = INADDR_ANY;
#endif
    if ( fixeddataport == 1 ) {
        data_source.sin_port = htons(ntohs(ctrl_addr.sin_port) - 1);
    } else {
        data_source.sin_port = 0;
    }
    if ( bind(sock, (struct sockaddr *) &data_source,sizeof(data_source)) < 0) {
        close(sock) ;
        syslog(BBFTPD_ERR,"Cannot bind on receive socket port %d : %s",portnumber,strerror(errno)) ;
        sprintf(logmessage,"Cannot bind on receive socket port %d : %s",portnumber,strerror(errno)) ;
        return (-1) ;
    }
    data_source.sin_addr = his_addr.sin_addr;
    data_source.sin_port = htons(portnumber);
    addrlen = sizeof(data_source) ;
    retcode = connect(sock,(struct sockaddr *) &data_source,addrlen) ;
    if ( retcode < 0 ) {
        if ( errno == EINTR ) {
            /*
            ** In this case we are going to tell the calling program 
            ** to retry so we close the socket and set the return code
            ** to zero 
            */
            close(sock) ;
            syslog(BBFTPD_ERR,"Cannot connect receive socket port %d : %s, telling calling to retry ",portnumber,strerror(errno)) ;
            return 0 ;
        } else {
            sprintf(logmessage,"Cannot connect receive socket port %s:%d : %s",inet_ntoa(his_addr.sin_addr),portnumber,strerror(errno)) ;
            syslog(BBFTPD_ERR,"Cannot connect receive socket port %s:%d : %s",inet_ntoa(his_addr.sin_addr),portnumber,strerror(errno)) ;
            close(sock) ;
            return (-1) ;
        }
    }
	syslog(BBFTPD_DEBUG,"Connected on port: %s:%d\n",inet_ntoa(his_addr.sin_addr),portnumber);
    return sock ;
}

/*******************************************************************************
** bbftpd_getdatasockk :                                                       *
**                                                                             *
**      Routine to create all the needed sockets to send data                  *
**                                                                             *
**      INPUT variable :                                                       *
**           nbsock   :  number of sockets to create    NOT MODIFIED           *
**                                                                             *
**      RETURN:                                                                *
**          -1   Creation failed unrecoverable error                           *
**           0   OK                                                            *
**                                                                             *
*******************************************************************************/
int bbftpd_getdatasock(int nbsock) 
{
    int     i ;
    int     tcpwinsize ;
    int     on = 1 ;
#if defined(SUNOS) || defined(_HPUX_SOURCE) || defined(IRIX)
    int     addrlen ;
#else
    socklen_t        addrlen ;
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


    mysockfree = mysockets ;
    myportfree = myports ;
    sck = ctrl_addr ;
    for (i=0 ; i<nbsock ; i++) {
        sck.sin_port = 0;
        *mysockfree = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ;
        if ( *mysockfree < 0 ) {
            syslog(BBFTPD_ERR, "Cannot create data socket : %s",strerror(errno));
			/*            sprintf(logmessage,"Cannot create data socket : %s",strerror(errno)) ;*/
            return -1 ;
        }
        if ( setsockopt(*mysockfree,SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)) < 0 ) {
            syslog(BBFTPD_ERR, "Cannot set SO_REUSEADDR on data socket : %s",strerror(errno));
			/*            sprintf(logmessage,"Cannot set SO_REUSEADDR on data socket : %s",strerror(errno)) ;*/
            return -1 ;
        }
        if ( setsockopt(*mysockfree,IPPROTO_TCP, TCP_NODELAY,(char *)&on,sizeof(on)) < 0 ) {
            syslog(BBFTPD_ERR, "Cannot set TCP_NODELAY on data socket : %s",strerror(errno));
			/*            sprintf(logmessage,"Cannot set TCP_NODELAY on data socket : %s",strerror(errno)) ;*/
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
                syslog(BBFTPD_WARNING, "Cannot set IP_TOS on data socket : %s",strerror(errno));
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
                syslog(BBFTPD_WARNING, "Unable to get send buffer size : %s\n",tcpwinsize,strerror(errno));
            } else {
                syslog(BBFTPD_WARNING, "Send buffer cannot be set to %d Bytes, Value is %d Bytes\n",1024*recvwinsize,tcpwinsize);
            }
        }
        tcpwinsize = 1024 * sendwinsize ;
        if ( setsockopt(*mysockfree,SOL_SOCKET, SO_RCVBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)) < 0 ) {
            addrlen = sizeof(tcpwinsize) ;
            if ( getsockopt(*mysockfree,SOL_SOCKET,SO_RCVBUF,(char *)&tcpwinsize,&addrlen) < 0 ) {
                syslog(BBFTPD_WARNING, "Unable to get receive buffer size : %s\n",tcpwinsize,strerror(errno));
            } else {
                syslog(BBFTPD_WARNING, "Receive buffer cannot be set to %d Bytes, Value is %d Bytes\n",1024*recvwinsize,tcpwinsize);
            }
        }    
        li.l_onoff = 1 ;
        li.l_linger = 1 ;
        if ( setsockopt(*mysockfree,SOL_SOCKET,SO_LINGER,(char *)&li,sizeof(li)) < 0 ) {
            syslog(BBFTPD_ERR, "Cannot set SO_LINGER on data socket : %s",strerror(errno));
			/*            sprintf(logmessage,"Cannot set SO_LINGER on data socket : %s",strerror(errno)) ;*/
            return -1 ;
        }
        if (pasvport_min) { /* Try to bind within a range */
 	        for (port=pasvport_min; port<=pasvport_max; port++) {
     	        sck.sin_port = htons(port);
 	            if (bind(*mysockfree, (struct sockaddr *) &sck, sizeof(sck)) >= 0) break;
 	        }
 	        if (port>pasvport_max) {
                syslog(BBFTPD_ERR, "Cannot bind on data socket : %s",strerror(errno));
                return -1 ;
 	        }
        } else {
            if (bind(*mysockfree, (struct sockaddr *) &sck, sizeof(sck)) < 0) {
                syslog(BBFTPD_ERR, "Cannot bind on data socket : %s",strerror(errno));
                return -1 ;
 	        }
		}
        addrlen = sizeof(sck) ;
        if (getsockname(*mysockfree,(struct sockaddr *)&sck, &addrlen) < 0) {
            syslog(BBFTPD_ERR, "Cannot getsockname on data socket : %s",strerror(errno));
            /*sprintf(logmessage,"Cannot getsockname on data socket : %s",strerror(errno)) ;*/
            return -1 ;
           }
        if (listen(*mysockfree, 1) < 0) {
            syslog(BBFTPD_ERR, "Cannot listen on data socket : %s",strerror(errno));
            /*sprintf(logmessage,"Cannot listen on data socket : %s",strerror(errno)) ;*/
            return -1 ;
        }
        *myportfree++ =  sck.sin_port ;
        syslog(BBFTPD_DEBUG,"Listen on port %d\n", ntohs(sck.sin_port)) ; 
        mysockfree++ ;
    }
    return 0 ;
}
