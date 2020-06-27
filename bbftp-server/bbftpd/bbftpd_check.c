/*
 * bbftpd/bbftpd_check.c
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

  
 Routines : void checkfromwhere()
            int checkprotocol()
  
 bbftpd_check.c v 2.0.0  2000/12/18 - Creation of the routine.
                v 2.0.1  2001/04/23 - Correct indentation
                v 2.1.0  2001/06/01 - Change file name
                                    - Reorganise routines as in bbftp_
                                     
 *****************************************************************************/
#include <bbftpd.h>

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include <syslog.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <unistd.h>
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <netinet/in.h>
#include <sys/socket.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <structures.h>
#include <version.h>

extern struct   sockaddr_in his_addr;
extern struct   sockaddr_in ctrl_addr;
extern  int     incontrolsock ;
extern  int     outcontrolsock ;
extern	int	recvcontrolto ;
extern	int	sendcontrolto ;
extern	int	checkstdinto ;
extern  int     protocolversion ;
extern  int     protocolmin ;
extern  int     protocolmax ;
extern  int     newcontrolport ; 
extern  int     fixeddataport;
extern  int     pasvport_min;
extern  int     pasvport_max;
extern  char    currentusername[MAXLEN] ;

/*******************************************************************************
** checkfromwhere :                                                            *
**                                                                             *
**      This routine get a socket on a port, send a MSG_LOGGED_STDIN and       *
**      wait for a connection on that port.                                    *
**                                                                             *
**      RETURN:                                                                *
**          No return but the routine exit in case of error.                   *
*******************************************************************************/


void checkfromwhere() 
{
    int        sock,ns,addrlen,retcode,nfds ;
    struct  sockaddr_in server ;
    char    buffer[MINMESSLEN] ;
     struct    message *msg ;
    struct  timeval wait_timer;
    fd_set    selectmask ;
    struct linger li ;
    int     on = 1 ;
    
    
    sock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP ) ;
    if ( sock < 0 ) {
        syslog(BBFTPD_ERR, "Cannot create checkreceive socket : %s",strerror(errno));
        reply(MSG_BAD,"Cannot create checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        exit(1) ;
    }
    if ( setsockopt(sock,SOL_SOCKET, SO_REUSEADDR,(char *)&on,sizeof(on)) < 0 ) {
        syslog(BBFTPD_ERR,"Cannot set SO_REUSEADDR on checkreceive socket : %s\n",strerror(errno)) ;
        reply(MSG_BAD,"Cannot set SO_REUSEADDR on checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(sock) ;
        exit(1) ;
    }
    li.l_onoff = 1 ;
    li.l_linger = 1 ;
    if ( setsockopt(sock,SOL_SOCKET,SO_LINGER,(char *)&li,sizeof(li)) < 0 ) {
        syslog(BBFTPD_ERR,"Cannot set SO_LINGER on checkreceive socket : %s\n",strerror(errno)) ;
        reply(MSG_BAD,"Cannot set SO_LINGER on checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(sock) ;
        exit(1) ;
    }
    
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    if ( fixeddataport == 1 ) {
        server.sin_port = htons(newcontrolport+1)  ;
    } else {
         server.sin_port = 0  ;
    }
    if (fixeddataport == 0 && pasvport_min) { /* Try to bind within a range */
        int port;
        for (port=pasvport_min; port<=pasvport_max; port++) {
            server.sin_port = htons(port);
            if (bind(sock,(struct sockaddr *) &server, sizeof(server) ) >= 0) break;
        }
        if (port>pasvport_max) {
            syslog(BBFTPD_ERR, "Cannot bind on data socket : %s",strerror(errno));
            reply(MSG_BAD,"Cannot bind checkreceive socket") ;
            syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
            close(sock) ;
            exit(1) ;
        }
    } else if ( bind (sock,(struct sockaddr *) &server, sizeof(server) ) < 0 ) {
        syslog(BBFTPD_ERR,"Error binding checkreceive socket : %s",strerror(errno)) ;
        reply(MSG_BAD,"Cannot bind checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(sock) ;
        exit(1) ;
    }
    addrlen = sizeof(server) ;
    if (getsockname(sock,(struct sockaddr *)&server, &addrlen) < 0) {
        syslog(BBFTPD_ERR,"Error getsockname checkreceive socket : %s",strerror(errno)) ;
        reply(MSG_BAD,"Cannot getsockname checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(sock) ;
        exit(1) ;
    }
    if ( listen(sock,1) < 0 ) {
        syslog(BBFTPD_ERR,"Error listening checkreceive socket : %s",strerror(errno)) ;
        reply(MSG_BAD,"Error listening checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(sock) ;
        exit(1) ;
    }
    syslog(BBFTPD_INFO, "listen port : %d",ntohs(server.sin_port));
    /*
    ** Send the MSG_LOGGED_STDIN message
    */
    msg = (struct message *)buffer ;
    msg->code = MSG_LOGGED_STDIN ;
#ifndef WORDS_BIGENDIAN
    msg->msglen = ntohl(4) ;
#else
    msg->msglen = 4 ;
#endif
    if ( writemessage(outcontrolsock,buffer,MINMESSLEN,sendcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error writing checkreceive socket part 1") ;
        reply(MSG_BAD,"Error writing checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(sock) ;
        exit(1) ;
    }
    /*
    ** Send the port number
    */
#ifndef WORDS_BIGENDIAN
    msg->code = ntohl(ntohs(server.sin_port)) ;
#else
    msg->code = server.sin_port ;
#endif
     if ( writemessage(outcontrolsock,buffer,4,sendcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error writing checkreceive socket part 2") ;
        reply(MSG_BAD,"Error writing checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        exit(1) ;
    }
    /*
    ** Now wait on the socket for a connection
    */
    nfds = sysconf(_SC_OPEN_MAX) ;
    FD_ZERO(&selectmask) ;
    FD_SET(sock,&selectmask) ;
    /*
    ** Set the timer for the connection
    */
    wait_timer.tv_sec  = checkstdinto ;
    wait_timer.tv_usec = 0 ;
    retcode = select(nfds,&selectmask,0,0,&wait_timer) ;
    if ( retcode < 0 ) {
        syslog(BBFTPD_ERR,"Error select checkreceive socket : %s ",strerror(errno)) ;
        reply(MSG_BAD,"Error select checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(sock) ;
        exit(1) ;
    }
    if ( retcode == 0 ) {
        /*
        ** Time out
        */
        syslog(BBFTPD_ERR,"Time out select checkreceive socket ") ;
        reply(MSG_BAD,"Time Out select checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(sock) ;
        exit(1) ;
    }
    if ( (ns = accept(sock,0,0) ) < 0 ) {
        syslog(BBFTPD_ERR,"Error accept checkreceive socket ") ;
        reply(MSG_BAD,"Error accep checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(sock) ;
        exit(1) ;
    }
    close(sock) ;
    addrlen = sizeof(his_addr);
    if (getpeername(ns, (struct sockaddr *) &his_addr, &addrlen) < 0) {
        syslog(BBFTPD_ERR, "getpeername : %s",strerror(errno));
        reply(MSG_BAD,"getpeername error") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(ns) ;
        exit(1);
    }
    addrlen = sizeof(ctrl_addr);
    if (getsockname(ns, (struct sockaddr *) &ctrl_addr, &addrlen) < 0) {
        syslog(BBFTPD_ERR, "getsockname : %s",strerror(errno));
        reply(MSG_BAD,"getsockname error") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(ns) ;
        exit(1);
    }
    /*
    ** Now read the message
    */
    if ( readmessage(ns,buffer,MINMESSLEN,recvcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error reading checkreceive socket") ;
        reply(MSG_BAD,"Error reading checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(ns) ;
        exit(1) ;
    }
    if (msg->code != MSG_IPADDR ) {
        syslog(BBFTPD_ERR,"Receive unkown message on checkreceive socket") ;
        reply(MSG_BAD,"Receive unkown message on checkreceive socket") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(ns) ;
        exit(1);
    }
    /*
    ** Everything seems OK so send a MSG_IPADDR_OK on the
    ** control socket and close the connection
    */
    msg->code = MSG_IPADDR_OK ;
    msg->msglen = 0 ;
     if ( writemessage(ns,buffer,MINMESSLEN,sendcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error writing checkreceive socket OK message") ;
        reply(MSG_BAD,"Error writing checkreceive socket OK message") ;
        syslog(BBFTPD_INFO,"User %s disconnected",currentusername) ;
        close(ns) ;
        exit(1) ;
    }
    /*
    ** set the port of ctrl_addr structure to the control port
    */
    ctrl_addr.sin_port = htons(newcontrolport) ;
    /*
    ** Wait a while before closing
    */
    close(ns) ;
}


/*******************************************************************************
** checkprotocol :                                                             *
**                                                                             *
**      This routine send it protocol version and wait for the client prorocol *
**      version.                                                               *
**                                                                             *
**      RETURN:                                                                *
**           0 OK                                                              *
**          -1 calling program exit                                            *
*******************************************************************************/

int checkprotocol() 
{

    char    buffer[MINMESSLEN] ;
    struct  message *msg ;
    int     msglen ;

    msg = ( struct    message * ) buffer ;
    msg->code = MSG_PROT_ANS ;
#ifndef WORDS_BIGENDIAN
    msg->msglen = ntohl(MINMESSLEN) ;
#else
    msg->msglen = MINMESSLEN ;
#endif
    if ( writemessage(outcontrolsock,buffer,MINMESSLEN,sendcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error writing MSG_PROT_ANS part 1") ;
        return -1 ;
    }
    /*
    ** Send the min and max protocol version
    */
#ifndef WORDS_BIGENDIAN
    msg->code = ntohl(protocolmin) ;
    msg->msglen = ntohl(protocolmax) ;
#else
    msg->code = protocolmin ;
    msg->msglen = protocolmax ;
#endif
     if ( writemessage(outcontrolsock,buffer,MINMESSLEN,sendcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error writing MSG_PROT_ANS part 2") ;
        return -1 ;
    }
    /*
    ** And wait for the answer
    */
    if ( readmessage(incontrolsock,buffer,MINMESSLEN,recvcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error waiting MSG_PROT_ANS") ;
        return -1 ;
    }
    if ( msg->code == MSG_PROT_ANS ) {
#ifndef WORDS_BIGENDIAN
        msglen = ntohl(msg->msglen) ;
#else 
        msglen = msg->msglen ;
#endif
        if ( msglen == 4 ) {
            if ( readmessage(incontrolsock,buffer,4,recvcontrolto) < 0 ) {
                syslog(BBFTPD_ERR,"Error waiting MSG_PROT_ANS (protocol version)") ;
                return -1 ;
            }
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->code) ;
#else 
            msglen = msg->code ;
#endif
            protocolversion = msglen ;
            return 0 ;
        } else {
            syslog(BBFTPD_ERR,"Unexpected length while MSG_PROT_ANS %d",msglen) ;
            return -1 ;
        }
    } else if ( msg->code == MSG_BAD_NO_RETRY ) {
        syslog(BBFTPD_ERR,"Incompatible server and client") ;
        return -1 ;
    } else {
        syslog(BBFTPD_ERR,"Unexpected message while MSG_PROT_ANS %d",msg->code) ;
        return -1 ;
    }

    
}
