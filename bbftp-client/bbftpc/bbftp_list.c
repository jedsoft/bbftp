/*
 * bbftpc/bbftp_list.c
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

  
  
 bbftp_list.c  v 2.0.0  2001/03/26  - Complete rewriting for version 2.0.0
               v 2.0.1  2001/04/19  - Correct indentation 


*****************************************************************************/
#include <bbftp.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
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
#if HAVE_STRING_H
# include <string.h>
#endif

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <netinet/in.h>
#include <structures.h>

extern  int     verbose ;
extern  int     timestamp ;
extern  int     transferoption  ; 
extern  int     incontrolsock ;
extern  int     outcontrolsock ;
extern	int	recvcontrolto ;
extern	int	sendcontrolto ;

int bbftp_list(char *line,char **filelist,int *filelistlen,int *errcode)
{

    char    minbuffer[MINMESSLEN] ;
    int     msglen ;
    int     code ;
    struct  message *msg ;
    struct  mess_integer *msg_integer ;
    fd_set  selectmask ; /* Select mask */
    int     nfds ; /* Max number of file descriptor */
    char    *buffer ;
    int     retcode ;
    
    if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : list %s\n",line) ;

    msg = (struct message *)minbuffer ;
    msg->code = MSG_LIST_V2 ;
#ifndef WORDS_BIGENDIAN
    msg->msglen = ntohl(strlen(line)+sizeof(int)) ;
#else
    msg->msglen = strlen(line)+sizeof(int) ;
#endif

    if ( writemessage(outcontrolsock,minbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
        /*
        ** We were not able to send the minimum message so
        ** we are going to close the control socket and to 
        ** tell the calling program to restart a connection
        */
        printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_LIST_V2");
        *errcode = 64 ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
    }
    /* 
    ** Sending transfer options
    */
    msg_integer = (struct mess_integer*)minbuffer ;
    msg_integer->myint =  transferoption ;
    if ( writemessage(outcontrolsock,minbuffer,sizeof(int),sendcontrolto,0) < 0 ) {
        printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_LIST_V2 (transferoption)");
        *errcode = 64 ;
        bbftp_close_control() ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
    }
    /* 
    ** Directory name
    */
    if ( writemessage(outcontrolsock,line,strlen(line),sendcontrolto,0) < 0 ) {
        printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_LIST_V2 (directory name)");
        *errcode = 64 ;
        bbftp_close_control() ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
    }
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
            *errcode = 66 ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ;
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
        if ( readmessage(incontrolsock,minbuffer,MINMESSLEN,recvcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,61,timestamp,"Error waiting %s message\n","MSG_OK (on MSG_LIST_V2)");
            *errcode = 61 ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ;
        }
        code = msg->code ;
        if ( code == MSG_BAD || code == MSG_BAD_NO_RETRY) {
            /*
            ** The change directory has failed so ...
            */
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            if ( (buffer = (char *) malloc(msglen+1) ) == NULL) {
                printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","buffer (bbftp_list)",strerror(errno)) ;
                *errcode = 35 ;
                bbftp_close_control() ;
                return BB_RET_CONN_BROKEN ;
            }
            if ( readmessage(incontrolsock,buffer,msglen,recvcontrolto,0) < 0 ) {
                printmessage(stderr,CASE_ERROR,67,timestamp,"Error reading data for %s message\n","MSG_BAD (on MSG_LIST_V2)");
                *errcode = 67 ;
                bbftp_close_control() ;
                free(buffer) ;
                if ( code == MSG_BAD ) {
                    return BB_RET_CONN_BROKEN ;
                } else {
                    return BB_RET_FT_NR_CONN_BROKEN ;
                }
            } else {
                buffer[msglen] = '\0' ;
                printmessage(stderr,CASE_ERROR,100,timestamp,"%s\n",buffer) ;
                if (verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< %s\n",buffer) ;
                free(buffer) ;
                if ( code == MSG_BAD ) {
                    *errcode = 100 ;
                    return BB_RET_ERROR;
                } else {
                    /*
                    ** In case of no retry we are going to wait 2 seconds
                    ** in order for the server to finish its cleaning (in
                    ** fact if the error message is du to a server child
                    ** abnormal termination, the message is sent on the death
                    ** of the first child and the server has some cleaning
                    ** to do).
                    */
                    sleep(2) ;
                    *errcode = 100 ;
                    return BB_RET_FT_NR ;
                }
            }
        } else if (msg->code == MSG_LIST_REPL_V2 ) {
            /*
            ** At this stage the transfer is OK but we need
            ** the answer because it gives all file names
            */
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            if ( msglen == 0 ) {
                /*
                ** There was no file corresponding
                */
                if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK : no file\n") ;
                *filelistlen = 0 ; 
                return BB_RET_OK ;
            } else {
                if ( ((*filelist) = (char *) malloc(msglen+1) ) == NULL) {
                    printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","filelist (bbftp_list)",strerror(errno)) ;
                    *errcode = 35 ;
                    bbftp_close_control() ;
                    return BB_RET_CONN_BROKEN ;
                }
                if ( readmessage(incontrolsock,*filelist,msglen,recvcontrolto,0) < 0) {
                    printmessage(stderr,CASE_ERROR,67,timestamp,"Error reading data for %s message\n","MSG_LIST_REPL_V2 (on MSG_LIST_V2)");
                    *errcode = 67 ;
                    bbftp_close_control() ;
                    free(*filelist) ;
                    return BB_RET_CONN_BROKEN ;
                }
                (*filelist)[msglen] = '\0' ;
                *filelistlen = msglen ; 
                if ( verbose ) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
                return BB_RET_OK ;
            }
        } else {
            /*
            ** Receive unkwown message so something is
            ** going wrong. close the control socket
            ** and restart
            */
            printmessage(stderr,CASE_ERROR,62,timestamp,"Unknown message while waiting for %s message\n","MSG_OK (on MSG_MKDIR_V2)");
            *errcode = 62 ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ;
        }
    }
}
