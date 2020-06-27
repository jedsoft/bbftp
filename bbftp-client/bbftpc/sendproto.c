/*
 * bbftpc/sendproto.c
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

  
 RETURN :
      0                  = everything OK 
     -1                 = Connection broken retry possible

  This routine may exit in case of retry not possible
   
 sendproto.c v 2.0.0  2001/02/10    - Creation of the routine 
             v 2.0.1  2001/04/19    - Correct indentation 
                                    - Port to IRIX
 
*****************************************************************************/
#include <bbftp.h>

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/stat.h>
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
#include <sys/types.h>
#include <unistd.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include "_bbftp.h"

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <structures.h>

int sendproto() 
{

    char    buffer[8] ;
    struct  message *msg ;
    int     remoteprotomin ;
    int     remoteprotomax ;
    fd_set  selectmask ; /* Select mask */
    int     nfds ; /* Max number of file descriptor */
    int     code ;
    int     retcode ;
    int     msglen ;
  

    if ( debug ) printmessage(stdout,CASE_NORMAL,0,timestamp,"Sending protocol request\n") ;
    /* 
    ** Now send the control message : First part
    */
    msg = (struct message *)buffer ;
    msg->code = MSG_PROT ;
    msg->msglen = 0 ;
    if ( writemessage(outcontrolsock,buffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
        /*
        ** We were not able to send the minimum message so
        ** we are going to close the control socket and to 
        ** tell the calling program to restart a connection
        */
        printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_PROT");
        return -1 ; /* restart connection */
    }
    if ( debug ) printmessage(stdout,CASE_NORMAL,0,timestamp,"Sent message %x\n", msg->code) ;
    /*
    ** Now we are going to wait for the message on the control 
    ** connection
    */
waitcontrol:
    nfds = sysconf(_SC_OPEN_MAX) ;
   (void) nfds;
    FD_ZERO(&selectmask) ;
    FD_SET(incontrolsock,&selectmask) ;
    retcode = select(FD_SETSIZE,&selectmask,0,0,0) ;
    if ( retcode < 0 ) {
        /*
        ** Select error
        */
        if ( errno != EINTR ) {
            /*
            ** we have got an error so close the connection
            ** and restart
            */
            printmessage(stderr,CASE_ERROR,66,timestamp,"Error select on control connection : %s\n",strerror(errno));
            return -1 ;
        } else {
            /*
            ** Interrupted by a signal
            */
            FD_ZERO(&selectmask) ;
            FD_SET(incontrolsock,&selectmask) ;
            goto waitcontrol ;
        }
    } else if ( retcode == 0 ) {
        /*
        ** Impossible we do not set any timer
        */
        FD_ZERO(&selectmask) ;
        FD_SET(incontrolsock,&selectmask) ;
        goto waitcontrol ;
    } else {
        /*
        ** read the message
        */
        if ( readmessage(incontrolsock,buffer,MINMESSLEN,recvcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,61,timestamp,"Error waiting %s message\n","MSG_PROT_ANS");
            return -1 ;
        }
        msg = (struct message *)buffer ;
        code = msg->code ;
        if ( code == MSG_BAD || code == MSG_BAD_NO_RETRY) {
            /*
            ** The server does not understand protocol 
            */
            printmessage(stderr,CASE_FATAL_ERROR,101,timestamp,"Incompatible deamon and client (remote protocol version (%d,%d), local (%d,%d))\n",1,1,protocolmin,protocolmax);
            
        } else if (msg->code == MSG_PROT_ANS ) {
            /*
            ** At this stage 
            */
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            if ( msglen != 8 ) {
                printmessage(stderr,CASE_ERROR,63,timestamp,"Unexpected message length while waiting for %s message\n","MSG_PROT_ANS");
                 return -1 ;
            }
            if ( readmessage(incontrolsock,buffer,msglen,recvcontrolto,0) < 0) {
                printmessage(stderr,CASE_ERROR,67,timestamp,"Error reading data for %s message\n","MSG_PROT_ANS");
                return -1 ;
            }
#ifndef WORDS_BIGENDIAN
            remoteprotomin = ntohl(msg->code) ;
            remoteprotomax = ntohl(msg->msglen) ;
#else
            remoteprotomin = msg->code ;
            remoteprotomax = msg->msglen ;
#endif            
            if ( (remoteprotomax < protocolmin) || (remoteprotomin > protocolmax) ) {
                /*
                ** Imcompatible version
                */
                msg->code = MSG_BAD_NO_RETRY ;
                msg->msglen = 0 ;
                writemessage(outcontrolsock,buffer,MINMESSLEN,recvcontrolto,0); 
                printmessage(stderr,CASE_FATAL_ERROR,101,timestamp,"Incompatible deamon and client (remote protocol version (%d,%d), local (%d,%d))\n",remoteprotomin,remoteprotomax,protocolmin,protocolmax);
               
            } else if (remoteprotomax <= protocolmax ) {
                protocol = remoteprotomax ;
            } else {
                protocol = protocolmax ;
            }
            msg->code = MSG_PROT_ANS ;
#ifndef WORDS_BIGENDIAN
            msg->msglen = ntohl(4) ;
#else
            msg->msglen = 4 ;
#endif
            if ( writemessage(outcontrolsock,buffer,MINMESSLEN,recvcontrolto,0) < 0 ){
                printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_PROT_ANS");
                return -1 ; /* restart connection */
            }
#ifndef WORDS_BIGENDIAN
            msg->code = ntohl(protocol) ;
#else
            msg->code = protocol ;
#endif
            if ( writemessage(outcontrolsock,buffer,4,recvcontrolto,0) < 0 ){
                printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_PROT_ANS");
                return -1 ; /* restart connection */
            }
            return 0 ;
        } else {
            /*
            ** Receive unkwown message so something is
            ** going wrong. close the control socket
            ** and restart
            */
            printmessage(stderr,CASE_ERROR,62,timestamp,"Unknown message while waiting for %s message\n","MSG_PROT_ANS");
            return -1 ;
        }
    }
    /*
    ** Never reach this point but to in order to avoid stupid messages
    ** from IRIX compiler set a return code to -1
    */
    return -1 ;

}
