/*
 * bbftpd/createreceivesock.c
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

  
 Create the receive socket and connect to it.

RETURN VALUE : 
 In case of failure no socket is opened and -1 is returned. 
 If connect fail due to interrupted system call 0 is returned.
 In case of success the socket is returned. 
  
 createreceivesock.c v 1.3.0 2000/03/20
                     v 1.4.0 2000/03/20 - Correct bug on sprintf ( forgot the 
                                           socknum)
                                          Bug shown by gfarrache
                     v 1.6.0 2000/03/24 - Set Send buffer to TCPWINSIZE for
                                          get option
                     v 1.6.1 2000/03/24 - Portage to OSF1
                                        - Set the socket in non block mode
                     v 1.8.7 2000/05/24 - Change headers
                     v 1.9.3 2000/10:12 - Use FIXEDDATAPORT in order to
                                          manage the local port
                     v 1.9.4 2000/10/16 - Make all sockets blocking in order
                                          to prevent the error in case of lack 
                                          of memory
                                        - Supress %m
                     v 2.0.1 2001/04/23 - Correct indentation
                     v 2.0.2 2001/05/04 - Correct include for RFIO

*****************************************************************************/
#include <bbftpd.h>

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <common.h>
#include <daemon.h>

extern struct sockaddr_in his_addr;        /* Remote adresse */
extern struct sockaddr_in ctrl_addr;    /* Local adresse */
extern int  fixeddataport ;

int createreceivesock(port,socknum,logmessage)
    int        port ;
    int        socknum ;
    char    *logmessage ;
{
    int    sock ;
    struct sockaddr_in data_source ;
    int    on = 1 ;
    int tcpwinsize = TCPWINSIZE ;
    int    addrlen ;
    int    retcode ;

    sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ;
    if ( sock < 0 ) {
        syslog(BBFTPD_ERR, "Cannot create receive socket %d : %s",socknum,strerror(errno));
        sprintf(logmessage,"Cannot create receive socket %d : %s",socknum,strerror(errno)) ;
        return (-1) ;
    }
    if ( setsockopt(sock,SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)) < 0 ) {
        close(sock) ;
        syslog(BBFTPD_ERR,"Cannot set SO_REUSEADDR on receive socket %d : %s",socknum,strerror(errno)) ;
        sprintf(logmessage,"Cannot set SO_REUSEADDR on receive socket %d : %s",socknum,strerror(errno)) ;
        return (-1) ;
    }
    if ( setsockopt(sock,SOL_SOCKET, SO_RCVBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)) < 0 ) {
        close(sock) ;
        syslog(BBFTPD_ERR,"Cannot set SO_RCVBUF on receive socket %d : %s",socknum,strerror(errno)) ;
        sprintf(logmessage,"Cannot set SO_RCVBUF on receive socket %d : %s",socknum,strerror(errno)) ;
        return (-1) ;
    }
    if ( setsockopt(sock,SOL_SOCKET, SO_SNDBUF,(char *)&tcpwinsize,sizeof(tcpwinsize)) < 0 ) {
        close(sock) ;
        syslog(BBFTPD_ERR,"Cannot set SO_SNDBUF on receive socket %d : %s",socknum,strerror(errno)) ;
        sprintf(logmessage,"Cannot set SO_SNDBUF on receive socket %d : %s",socknum,strerror(errno)) ;
        return (-1) ;
    }
    if ( setsockopt(sock,IPPROTO_TCP, TCP_NODELAY,(char *)&on,sizeof(on)) < 0 ) {
        close(sock) ;
        syslog(BBFTPD_ERR,"Cannot set TCP_NODELAY on receive socket %d : %s",socknum,strerror(errno)) ;
        sprintf(logmessage,"Cannot set TCP_NODELAY on receive socket %d : %s",socknum,strerror(errno)) ;
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
        syslog(BBFTPD_ERR,"Cannot bind on receive socket %d : %s",socknum,strerror(errno)) ;
        sprintf(logmessage,"Cannot bind on receive socket %d : %s",socknum,strerror(errno)) ;
        return (-1) ;
    }
    data_source.sin_addr = his_addr.sin_addr;
    data_source.sin_port = htons(port);
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
            syslog(BBFTPD_ERR,"Cannot connect receive socket %d : %s, telling calling to retry ",socknum,strerror(errno)) ;
            return 0 ;
        } else {
            close(sock) ;
            syslog(BBFTPD_ERR,"Cannot connect receive socket %d : %s",socknum,strerror(errno)) ;
            sprintf(logmessage,"Cannot connect receive socket %d : %s",socknum,strerror(errno)) ;
            return (-1) ;
        }
    }
    return sock ;
}
