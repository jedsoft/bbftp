/*
 * bbftpc/bbftp_cert.c
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

  
  
 bbftp_cert.c  v 2.2.0 2001/10/03    - Routines creation

*****************************************************************************/
#include <bbftp.h>

#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
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
#if HAVE_STRING_H
# include <string.h>
#endif

#include "_bbftp.h"

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <structures.h>
#include <gssapi.h>
/* #include <gfw.h> */

/*******************************************************************************
** bbftp_cert_connect :                                                     *
**                                                                             *
**      This routine opens a connection with the server and simply calls       *
**      the GSS Framework (GFW) library to validate user's certificates        *
**      routine returns 0 to set up correctly the global parameters            *
**                                                                             *
**      OUPUT variable :                                                       *
**                                                                             *
**      GLOBAL VARIABLE USED :                                                 *                                                                      *
**          BBftp_Incontrolsock       MAY BE MODIFIED                                *
**          BBftp_Outcontrolsock      MAY BE MODIFIED                                *
**          hisrsa              MAY BE MODIFIED                                *
**          tmpctrlsock        MODIFIED                                       *
**                                                                             *
**                                                                             *
**      RETURN:                                                                *
**          -1  Unrecoverable error                                            *
**           0  OK                                                             *
**                                                                             *
*******************************************************************************/

int bbftp_cert_connect(void)

{
#if defined(SUNOS) || defined(_HPUX_SOURCE) || defined(IRIX)
    int     addrlen ;
#else
    socklen_t addrlen ;
#endif
    int     tmpctrlsock ;
    char    minbuffer[CRYPTMESSLEN] ;
    struct  message     *msg ;
    char    *readbuffer ;
    int     msglen ;
    int     code ; 
    OM_uint32 maj_stat, min_stat;

    BBftp_His_Ctladdr.sin_family = AF_INET;
    BBftp_His_Ctladdr.sin_port = htons(BBftp_Newcontrolport);
    if ( (tmpctrlsock = socket ( AF_INET, SOCK_STREAM, IPPROTO_TCP )) < 0 ) {
        printmessage(stderr,CASE_ERROR,51,  "Cannot create control socket : %s\n",strerror(errno));
        return -1 ;
    }
    /*
    ** Connect to the server
    */
    addrlen = sizeof(BBftp_His_Ctladdr) ;
    while ( connect(tmpctrlsock,(struct sockaddr*)&BBftp_His_Ctladdr,addrlen) < 0 ) {
       if (errno == EINTR) continue;
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,52,  "Cannot connect to control socket: %s\n",strerror(errno));
        return -1 ;
    }
    /*
    ** Get the socket name
    */
    addrlen = sizeof(BBftp_My_Ctladdr) ;
    if (getsockname(tmpctrlsock,(struct sockaddr*) &BBftp_My_Ctladdr, &addrlen) < 0) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,53, "Error getsockname on control socket: %s\n",strerror(errno)) ;
        return -1 ;
    }
    /*
    ** Connection is correct get the encryption
    */

	if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0, "Connection established\n") ;
    /*
    **    Read the encryption supported
    */
    if ( readmessage(tmpctrlsock,minbuffer,MINMESSLEN,BBftp_Recvcontrolto,0) < 0 ) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,54, "Error reading encryption message\n") ;
        return -1 ;
    }
    msg = (struct message *) minbuffer ;
    if ( msg->code != MSG_CRYPT) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,55, "No encryption message \n") ;
        return -1 ;
    }
#ifndef WORDS_BIGENDIAN
    msglen = ntohl(msg->msglen) ;
#else
    msglen = msg->msglen ;
#endif
    if ( ( readbuffer = (char *) malloc (msglen + 1) ) == NULL ) {
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,54, "Error reading encryption message : malloc failed (%s)\n",strerror(errno)) ;
        return -1 ;
    }
    if ( readmessage(tmpctrlsock,readbuffer,msglen,BBftp_Recvcontrolto,0) < 0 ) {
        free(readbuffer) ;
        close(tmpctrlsock) ;
        printmessage(stderr,CASE_ERROR,56, "Error reading encrypted message : %s\n","type") ;
        return -1 ;
    }
        msg = (struct message *) minbuffer ;
        msg->code = MSG_CERT_LOG ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(CRYPTMESSLEN) ;
#else
        msg->msglen = CRYPTMESSLEN ;
#endif
        if ( writemessage(tmpctrlsock,minbuffer,MINMESSLEN,BBftp_Recvcontrolto,0) < 0) {
            free(readbuffer) ;
            close(tmpctrlsock) ;
            printmessage(stderr,CASE_ERROR,58, "Error sending CERT_LOG message : %s\n",strerror(errno)) ;
            return -1 ;
        }
		/* 
		** The authentication process is run
        */
        if (!BBftp_Service) { 
            maj_stat = gfw_init_sec_context(&min_stat, tmpctrlsock, tmpctrlsock, BBftp_Hostent->h_name);
        } else {
            maj_stat = gfw_init_sec_context(&min_stat, tmpctrlsock, tmpctrlsock, BBftp_Service);
        }
        if (maj_stat != GSS_S_COMPLETE && maj_stat != -1) {
            gfw_msgs_list *messages = NULL;
            gfw_status_to_strings(maj_stat, min_stat, &messages) ;
            close(tmpctrlsock) ;
            if (BBftp_Verbose) {
                while (messages != NULL) {
                    printmessage(stderr,CASE_ERROR,27, "%s\n", messages->msg);
                    messages = messages->next;
                }
            } else {
                printmessage(stderr,CASE_ERROR,27, "%s\n", messages->msg);
            }
            return -1 ;
        } else {
            /*
            ** Certificate authentication seems OK on client side so wait for the MSG_OK
            ** message
            */
            if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0, "Client certificate authentication OK\n") ;
            if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0, "Waiting for server answer\n") ;
            if ( readmessage(tmpctrlsock,minbuffer,MINMESSLEN,BBftp_Recvcontrolto,0) < 0 ) {
                close(tmpctrlsock) ;
                free(readbuffer) ;
                printmessage(stderr,CASE_ERROR,59, "Error reading login message answer : %s\n","") ;
                return -1 ;
            }
            msg = (struct message *) minbuffer ;
            code = msg->code ;
            if ( code == MSG_BAD || code == MSG_BAD_NO_RETRY) {
#ifndef WORDS_BIGENDIAN
                msglen = ntohl(msg->msglen) ;
#else
                msglen = msg->msglen ;
#endif
                if ( ( readbuffer = (char *) malloc(msglen + 1) ) == NULL ) {
                    printmessage(stderr,CASE_ERROR,59, "Error reading login message answer : malloc failed (%s)\n",strerror(errno)) ;
                    return -1 ;
                }
                if ( readmessage(tmpctrlsock,readbuffer,msglen,BBftp_Recvcontrolto,0) < 0 ) {
                    close(tmpctrlsock) ;
                    free(readbuffer) ;
                    if ( code == MSG_BAD ) {
                        printmessage(stderr,CASE_ERROR,59, "Error reading login message answer : %s\n","BAD message") ;
                        return -1 ;
                    } else {
                        printmessage(stderr,CASE_FATAL_ERROR,59, "Error reading login message answer : %s\n","BAD NO RETRY message") ;
                    }
                } else {
                    close(tmpctrlsock) ;
                    readbuffer[msglen] = '\0' ;
                    if ( code == MSG_BAD ) {
                        printmessage(stderr,CASE_ERROR,100, "%s\n",readbuffer) ;
                        free(readbuffer) ;
                        return -1 ;
                    } else {
                        printmessage(stderr,CASE_FATAL_ERROR,100, "%s\n",readbuffer) ;
                    }
                }
            } else if ( code == MSG_OK || code == MSG_WARN) {
#ifndef WORDS_BIGENDIAN
                msglen = ntohl(msg->msglen) ;
#else
                msglen = msg->msglen ;
#endif
                if ( ( readbuffer = (char *) malloc(msglen + 1) ) == NULL ) {
                    printmessage(stderr,CASE_ERROR,59, "Error reading login message answer : OK message : malloc failed (%s)\n",strerror(errno)) ;
                    return -1 ;
                }
                if ( readmessage(tmpctrlsock,readbuffer,msglen,BBftp_Recvcontrolto,0) < 0 ) {
                    free(readbuffer) ;
                    close(tmpctrlsock) ;
                    printmessage(stderr,CASE_ERROR,59, "Error reading login message answer : %s\n","OK message") ;
                    return -1 ;
                } else {
                    readbuffer[msglen] = '\0' ;
                    if ( code == MSG_OK ) {
                        if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< %s\n",readbuffer) ;
                    } else {
                        if ( BBftp_Warning) printmessage(stdout,CASE_WARNING,100, " %s\n",readbuffer) ;
                    }
                }
            } else {
                free(readbuffer) ;
                close(tmpctrlsock) ;
                printmessage(stderr,CASE_ERROR,59, "Error reading login message answer : %s\n","Unkown answer message") ;
                return -1 ;
            }
            if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0, "Server answer : OK\n") ;
            free(readbuffer) ;
            BBftp_Incontrolsock  = tmpctrlsock ;
            BBftp_Outcontrolsock = tmpctrlsock ;
            return 0 ;
        }
}


