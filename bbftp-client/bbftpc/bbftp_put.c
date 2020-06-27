/*
 * bbftpc/bbftp_put.c
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
    BB_RET_OK                   = everything OK 
    BB_RET_FT_NR                = Fatal error no retry needed on this command but 
                                  connection has been kept
    BB_RET_ERROR                = Error but retry possible
    BB_RET_FT_NR_CONN_BROKEN    = Fatal error no retry needed connection broken

 bbftp_put.c v 2.0.0 2001/03/13 - Creation of the routine 
             v 2.0.1 2001/04/19 - Correct indentation
             v 2.1.0 2001/05/29 - Add -m option
 
*****************************************************************************/
#include <bbftp.h>

#include <errno.h>

#include <signal.h>
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
#include <sys/wait.h>
#include <unistd.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include "_bbftp.h"

#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <netinet/in.h>
#include <structures.h>

#ifndef HAVE_NTOHLL
my64_t ntohll(my64_t v) ;
#endif

int bbftp_put(char *remotefilename,int  *errcode) 
{
    int     retcode ;
    char    sendbuffer[STORMESSLEN_V2] ;
    struct  message *msg ;
    struct  mess_store_v2 *msg_store_v2 ;
    fd_set  selectmask ; /* Select mask */
    int     nfds ; /* Max number of file descriptor */
    struct  timeval wait_timer;
    char    logmessage[1024] ;
    char    *buffer ;
    int     msglen ;
    int     code ;
    int     i,k ;
    int     *pidfree ;
    int     *mysockfree ;
    struct  timeval  tstart, tend ,tdiff;
    float   s, bs;
    int     *porttosend, *portfree ;
    int     status ;
    pid_t   pid ;
  
    if ( verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,">> COMMAND : put %s %s\n",curfilename,remotefilename) ;
    /*
    ** Check if file is correct 
    */
    if ( (retcode = bbftp_retrcheckfile(realfilename,logmessage,errcode)) < 0 ) {
        printmessage(stderr,CASE_ERROR,*errcode,timestamp,"%s\n",logmessage);
        bbftp_free_all_var() ;
        return  BB_RET_FT_NR ;
    } else if ( retcode > 0 ) {
        printmessage(stderr,CASE_ERROR,*errcode,timestamp,"%s\n",logmessage);
        bbftp_free_all_var() ;
        return BB_RET_ERROR ;
    }
    msg = (struct message *)sendbuffer ;
    msg->code = MSG_STORE_V2 ;
#ifndef WORDS_BIGENDIAN
    msg->msglen = ntohl(STORMESSLEN_V2) ;
#else
    msg->msglen = STORMESSLEN_V2 ;
#endif
     if ( writemessage(outcontrolsock,sendbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
        /*
        ** We were not able to send the minimum message so
        ** we are going to close the control socket and to 
        ** tell the calling program to restart a connection
        */
        printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_STORE_V2");
        *errcode = 64 ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
    }
    msg_store_v2 = (struct mess_store_v2 *)sendbuffer ;
    /*
    ** Set up the parameters
    */
    if ( debug ) printmessage(stdout,CASE_NORMAL,0,timestamp,"Accepted stream number = %d\n",requestedstreamnumber) ;
    msg_store_v2->transferoption = transferoption ;
    sprintf(msg_store_v2->lastaccess,"%s",lastaccess) ;
    sprintf(msg_store_v2->lastmodif,"%s",lastmodif) ;
#ifndef WORDS_BIGENDIAN
    msg_store_v2->filemode             = ntohl(filemode) ;
    msg_store_v2->sendwinsize          = ntohl(sendwinsize) ;
    msg_store_v2->recvwinsize          = ntohl(recvwinsize) ;
    msg_store_v2->buffersizeperstream  = ntohl(buffersizeperstream) ;
    msg_store_v2->nbstream             = ntohl(requestedstreamnumber) ;
    msg_store_v2->filesize             = ntohll(filesize) ;
#else
    msg_store_v2->filemode             = filemode ;
    msg_store_v2->sendwinsize          = sendwinsize ;
    msg_store_v2->recvwinsize          = recvwinsize ;
    msg_store_v2->buffersizeperstream  = buffersizeperstream ;
    msg_store_v2->nbstream             = requestedstreamnumber;
    msg_store_v2->filesize             = filesize ;
#endif
    if ( writemessage(outcontrolsock,sendbuffer,STORMESSLEN_V2,sendcontrolto,0) < 0 ) {
        /*
        ** We were not able to send the minimum message so
        ** we are going to close the control socket and to 
        ** tell the calling program to restart a connection
        */
        printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_STORE_V2 (data)");
        *errcode = 64 ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
    }
    /*
    ** Now send the file name
    */
    msg = (struct message *)sendbuffer ;
    msg->code = MSG_FILENAME ;
#ifndef WORDS_BIGENDIAN
    msg->msglen = ntohl(strlen(remotefilename)) ;
#else
    msg->msglen =  strlen(remotefilename);
#endif
    if ( writemessage(outcontrolsock,sendbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
        printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_FILENAME");
        *errcode = 64 ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
    }
    if ( writemessage(outcontrolsock,remotefilename,strlen(remotefilename),sendcontrolto,0) < 0 ) {
        printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_FILENAME (filename)");
        *errcode = 64 ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
    }
    /*
    ** At this stage we are waiting for a MSG_TRANS_OK_V2 MSG_OK
    ** MSG_BAD MSG_BAD_NO_RETRY
    */
waitcontrol1:
    nfds = sysconf(_SC_OPEN_MAX) ;
   (void) nfds;
    FD_ZERO(&selectmask) ;
    FD_SET(incontrolsock,&selectmask) ;
    wait_timer.tv_sec  = 100 ;
    wait_timer.tv_usec = 0 ;
    retcode = select(FD_SETSIZE,&selectmask,0,0,&wait_timer) ;
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
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ;
        } else {
            /*
            ** Interrupted by a signal
            */
            FD_ZERO(&selectmask) ;
            FD_SET(incontrolsock,&selectmask) ;
            goto waitcontrol1 ;
        }
    } else if ( retcode == 0 ) {
        goto waitcontrol1 ;
    } else {
        /*
        ** read the message
        */
        msg = (struct message *)sendbuffer ;
        if ( readmessage(incontrolsock,sendbuffer,MINMESSLEN,recvcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,61,timestamp,"Error waiting %s message\n","MSG_OK (on MSG_STORE_V2)");
            *errcode = 61 ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ;
        }
        msg = (struct message *)sendbuffer ;
        code = msg->code ;
        if ( code == MSG_BAD || code == MSG_BAD_NO_RETRY) {
            /*
            ** The transfer has failed so ...
            */
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            if ( (buffer = (char *) malloc(msglen+1) ) == NULL) {
                printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","buffer 1 (bbftp_put)",strerror(errno)) ;
                *errcode = 35 ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                return BB_RET_CONN_BROKEN ;
            }
            if ( readmessage(incontrolsock,buffer,msglen,recvcontrolto,0) < 0 ) {
                printmessage(stderr,CASE_ERROR,67,timestamp,"Error reading data for %s message\n","MSG_BAD (on MSG_STORE_V2)");
                *errcode = 67 ;
                bbftp_free_all_var() ;
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
                *errcode = 100 ;
                if ( code == MSG_BAD ) {
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
                    bbftp_free_all_var() ;
                    return BB_RET_FT_NR ;
                }
            }
        } else if (code == MSG_TRANS_OK_V2  || code == MSG_TRANS_OK_V3 ) {
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            if ( (buffer = (char *) malloc(msglen+1) ) == NULL) {
                printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","buffer 2 (bbftp_put)",strerror(errno)) ;
                *errcode = 35 ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                return BB_RET_CONN_BROKEN ;
            }
            if ( readmessage(incontrolsock,buffer,msglen,recvcontrolto,0) < 0 ) {
                printmessage(stderr,CASE_ERROR,67,timestamp,"Error reading data for %s message\n","MSG_TRANS_OK_V2");
                *errcode = 67 ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                free(buffer) ;
                return BB_RET_CONN_BROKEN ;
            }
            if (code == MSG_TRANS_OK_V3 ) { /* PASSIVE MODE: get the ports */
                msg_store_v2 = (struct mess_store_v2 *)buffer ;
#ifndef WORDS_BIGENDIAN
                requestedstreamnumber   = ntohl(msg_store_v2->nbstream) ;
#else
                requestedstreamnumber   = msg_store_v2->nbstream ;
#endif
                if ( (myports = (int *) malloc (requestedstreamnumber*sizeof(int)) ) == NULL ) {
                    printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","ports buffer",strerror(errno)) ;
                    *errcode = 35 ;
                    bbftp_free_all_var() ;
                    bbftp_close_control() ;
                    free(buffer);
                    return BB_RET_CONN_BROKEN ;
                }
                if ( readmessage(incontrolsock,(char *) myports,requestedstreamnumber*sizeof(int),recvcontrolto,0) < 0 ) {
                    printmessage(stderr,CASE_ERROR,68,timestamp,"Error reading ports for %s message\n","MSG_TRANS_OK_V3");
                    *errcode = 68 ;
                    bbftp_free_all_var() ;
                    bbftp_close_control() ;
                    free(buffer) ;
                    return BB_RET_CONN_BROKEN ;
                } else {
                    portfree = myports ;
                    for (i=0 ; i < requestedstreamnumber ; i++ ) {
#ifndef WORDS_BIGENDIAN
                        *portfree = ntohl(*portfree) ;
#endif
                        if ( debug ) printmessage(stdout,CASE_NORMAL,0,timestamp,"Reading port:%d\n",*portfree);
                        portfree++ ;
                    }
				}
			}
        } else if ( code == MSG_OK) {
            /*
            ** This case can only happend if filesize = 0 
            */
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            discardmessage(incontrolsock,msglen,recvcontrolto,0) ;
            if ( verbose ) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
            bbftp_free_all_var() ;
            return BB_RET_OK ;
        } else {
            /*
            ** Receive unkwown message so something is
            ** going wrong. close the control socket
            ** and restart
            */
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            printmessage(stderr,CASE_ERROR,62,timestamp,"Unknown message (%d,%d) while waiting for %s message\n",code,msglen,"MSG_TRANS_OK_V2");
            *errcode = 62 ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ;
        } 
    }
     msg_store_v2 = (struct mess_store_v2 *)buffer ;
    /*
    ** The only interesting thing is the requestedstreamnumber
    ** No: if the server cannot uncompress, it modifies the options
    */
#ifndef WORDS_BIGENDIAN
    requestedstreamnumber   = ntohl(msg_store_v2->nbstream) ;
#else
    requestedstreamnumber   = msg_store_v2->nbstream ;
#endif
    if ( (transferoption & TROPT_GZIP ) == TROPT_GZIP 
        && (msg_store_v2->transferoption & TROPT_GZIP) != TROPT_GZIP ) {
        if (warning) printmessage(stderr,CASE_WARNING,27,timestamp,"The server cannot handle compression: option ignored\n");
        transferoption = transferoption & ~TROPT_GZIP ;
    }
    free(buffer) ;
   /*
    ** Reserving memory to send the port numbers
	** Not used in passive mode but necessary to avoid freeing something NULL later
    */
    if ( (buffer = (char *) malloc (requestedstreamnumber*sizeof(int))) == NULL) {
        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","buffer 3 (bbftp_put)",strerror(errno));
        *errcode = 35 ;
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(outcontrolsock,sendbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_free_all_var() ;
        return BB_RET_ERROR ;
	}
    /*
    ** We are reserving the memory for the children pid
    */
    if ( (mychildren = (int *) malloc (requestedstreamnumber*sizeof(int))) == NULL) {
        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","mychildren (bbftp_get)",strerror(errno));
        *errcode = 35 ;
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(outcontrolsock,sendbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            free(buffer) ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
    }
    /*
    ** Setting mychildren to 0
    */
    pidfree = mychildren ;
    for (i=1; i<=requestedstreamnumber; i++ ) {
        *pidfree++ = 0 ;
    }
    /*
    ** We are reserving the memory for the readbuffer
    */
    if ( (readbuffer = (char *) malloc (buffersizeperstream*1024)) == NULL) {
        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","readbuffer (bbftp_get)",strerror(errno));
        *errcode = 35 ;
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(outcontrolsock,sendbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            free(buffer) ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
    }
    /*
    ** We are reserving the memory for the compression buffer 
    ** if needed
    */
#ifdef WITH_GZIP
    if ( (transferoption & TROPT_GZIP ) == TROPT_GZIP ) {
        if ( (compbuffer = (char *) malloc (buffersizeperstream*1024)) == NULL) {
            printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","compbuffer (bbftp_get)",strerror(errno));
            *errcode = 35 ;
            msg = (struct message *)sendbuffer ;
            msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
            msg->msglen = ntohl(0) ;
#else
            msg->msglen = 0;
#endif
            if ( writemessage(outcontrolsock,sendbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
                printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_ABORT");
                *errcode = 64 ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                free(buffer) ;
                return BB_RET_CONN_BROKEN ; /* restart connection */
            }
            bbftp_free_all_var() ;
            free(buffer) ;
            return BB_RET_ERROR ;
        }
    }
#endif
    /*
    ** Now getting space for the port
    */
    if ( protocol == 2 ) { /* ACTIVE MODE */
      if ( (myports = (int *) malloc (requestedstreamnumber*sizeof(int)) ) == NULL ) {
        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","myports (bbftp_get)",strerror(errno));
        *errcode = 35 ;
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(outcontrolsock,sendbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            free(buffer) ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
	  }
	}
    if ( (mysockets = (int *) malloc (requestedstreamnumber*sizeof(int)) ) == NULL ) {
        printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","mysockets (bbftp_get)",strerror(errno));
        *errcode = 35 ;
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(outcontrolsock,sendbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            free(buffer) ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
    }
/* BUG AIX 5.2 structure mysockets : bad initialisation */
      mysockfree = mysockets ;
      for ( i=0 ; i< requestedstreamnumber ; i++ ) {
        *mysockfree++ = 0 ;
      }
    if (protocol == 2) { /* ACTIVE MODE */
      if ( getdatasock(requestedstreamnumber,errcode) < 0 ) {
        /*
        ** In case of error getdatasock does not clean the
        ** structure and does not close the sockets, it is
        ** the calling program's work
        */ 
        mysockfree = mysockets ;
        for ( i=0 ; i< requestedstreamnumber ; i++ ) {
            if ( *mysockfree > 0 ) close(*mysockfree) ;
            mysockfree++ ;
        }
         msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(outcontrolsock,sendbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            free(buffer) ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
      }
      /*
      ** Now we are going to start the children
      */
      if ( (retcode = bbftp_retrtransferfile(realfilename,logmessage,errcode)) < 0 ) {
        /*
        ** At this point :  all children have been killed but sockets are not closed
        **                  file is deleted
        */
        mysockfree = mysockets ;
        for ( i=0 ; i< requestedstreamnumber ; i++ ) {
            if ( *mysockfree > 0 ) close(*mysockfree) ;
            mysockfree++ ;
        }
        printmessage(stderr,CASE_ERROR,*errcode,timestamp,"%s\n",logmessage);
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
      }
      /*
      ** Close all socket only needed by children
      */
      mysockfree = mysockets ;
      for ( i=0 ; i< requestedstreamnumber ; i++ ) {
        if ( *mysockfree > 0 ) close(*mysockfree) ;
        mysockfree++ ;
      }
      /*
      ** All children are correctly started so send the
      ** START message
      */ 
      msg = (struct message *)sendbuffer ;
      if (simulation_mode) {
        msg->code = MSG_TRANS_SIMUL ;
      } else {
        msg->code = MSG_TRANS_START_V2 ;
      }
#ifndef WORDS_BIGENDIAN
      msg->msglen = ntohl(requestedstreamnumber*sizeof(int)) ;
#else
      msg->msglen = requestedstreamnumber*sizeof(int) ;
#endif
      if ( writemessage(outcontrolsock,sendbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
        /*
        ** We were not able to send the minimum message so
        ** we are going to close the control socket and to 
        ** tell the calling program to restart a connection
        */
        printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_TRANS_START_V2");
        *errcode = 64 ;
        /*
        ** First kill all child
        */
        bbftp_clean_child() ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        free(buffer) ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
      }
      /* 
      ** Second part
      */
      portfree = (int *)buffer ;
      porttosend = myports ;
      for (i=0 ; i<requestedstreamnumber  ; i++) {
#ifndef WORDS_BIGENDIAN
        *portfree++  = ntohl(ntohs(*porttosend++)) ;
#else
        *portfree++ = *porttosend++ ;
#endif
      }

      if ( writemessage(outcontrolsock,buffer,requestedstreamnumber*sizeof(int),sendcontrolto,0) < 0) {
        printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_TRANS_START_V2 (ports)");
        *errcode = 64 ;
        /*
        ** First kill all child
        */
        bbftp_clean_child() ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        free(buffer) ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
      }
      free(buffer) ;
	} else { /* PASSIVE MODE */
      /*
      ** Now we are going to start the children
      */
      if ( (retcode = bbftp_retrtransferfile(realfilename,logmessage,errcode)) < 0 ) {
        /*
        ** At this point :  all children have been killed but sockets are not closed
        **                  file is deleted
        */
        mysockfree = mysockets ;
        for ( i=0 ; i< requestedstreamnumber ; i++ ) {
            if ( *mysockfree > 0 ) close(*mysockfree) ;
            mysockfree++ ;
        }
        printmessage(stderr,CASE_ERROR,*errcode,timestamp,"%s\n",logmessage);
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
      }
      /*
      ** Close all socket only needed by children
      */
      mysockfree = mysockets ;
      for ( i=0 ; i< requestedstreamnumber ; i++ ) {
        if ( *mysockfree > 0 ) close(*mysockfree) ;
        mysockfree++ ;
      }
      /*
      ** All children are correctly started so send the
      ** START message
      */ 
      msg = (struct message *)sendbuffer ;
      if (simulation_mode) {
        msg->code = MSG_TRANS_SIMUL_V3 ;
      } else {
        msg->code = MSG_TRANS_START_V3 ;
      }
#ifndef WORDS_BIGENDIAN
      msg->msglen = ntohl(requestedstreamnumber*sizeof(int)) ;
#else
      msg->msglen = requestedstreamnumber*sizeof(int) ;
#endif
      if ( writemessage(outcontrolsock,sendbuffer,MINMESSLEN,sendcontrolto,0) < 0 ) {
        /*
        ** We were not able to send the minimum message so
        ** we are going to close the control socket and to 
        ** tell the calling program to restart a connection
        */
        printmessage(stderr,CASE_ERROR,64,timestamp,"Error sending %s message\n","MSG_TRANS_START_V3");
        *errcode = 64 ;
        /*
        ** First kill all child
        */
        bbftp_clean_child() ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        free(buffer) ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
      }
      free(buffer) ;
    }
    /*
    ** now wait for the end of the game
    */
    (void) gettimeofday(&tstart, (struct timezone *)0);
    k = 0 ;
waitcontrol:
    nfds = sysconf(_SC_OPEN_MAX) ;
    FD_ZERO(&selectmask) ;
    FD_SET(incontrolsock,&selectmask) ;
    wait_timer.tv_sec  = CHECKCHILDTIME ;
    wait_timer.tv_usec = 0 ;
    retcode = select(FD_SETSIZE,&selectmask,0,0,&wait_timer) ;
    if ( retcode < 0 ) {
        /*
        ** Select error
        */
        if ( errno != EINTR ) {
            /*
            ** we have got an error so close the connection
            ** and restart
            */
            printmessage(stderr,CASE_ERROR,66,timestamp,"Error select on control connection : %s\n",strerror(errno) );
            *errcode = 66 ;
            /*
            ** Kill all child 
            */
            bbftp_clean_child() ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        } else {
            /*
            ** Interrupted by a signal
            */
            goto waitcontrol ;
        }
    } else if ( retcode == 0 ) {
        /*
        ** Check if all child are not dead
        */
        pidfree = mychildren ;
        for ( i = 0 ; i<requestedstreamnumber; i++) {
            if ( *pidfree != 0 ) {
                pid = waitpid(*pidfree,&status,WNOHANG) ;
                if ( pid < 0 ) {
                    if ( errno != EINTR ) {
                        if ( errno == ECHILD ) {
                            /*
                            ** Chid has terminated
                            */
                            *pidfree = 0 ;
                        }
                    }
                }
            }
            pidfree++ ;
        }
        pidfree = mychildren ;
        for ( i = 0 ; i<requestedstreamnumber; i++) {
            if ( *pidfree++ != 0 ) {
                /*
                ** There is still a child so continue waiting on 
                ** control socket
                */
                goto waitcontrol ;
            }
        }
        if ( k == 0 ) {
            /*
            ** First time we get here let a chance
            ** to get an answer on control socket
            */
            k = 1 ; 
            goto waitcontrol ;
        }
        /*
        ** Second time without any child 
        ** Bad thing so abort 
        */
        printmessage(stderr,CASE_ERROR,37,timestamp,"No more child\n");
        *errcode = 37 ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
    } else {
        /*
        ** read the message
        */
        if ( readmessage(incontrolsock,sendbuffer,MINMESSLEN,recvcontrolto,0) < 0 ) {
            printmessage(stderr,CASE_ERROR,61,timestamp,"Error waiting %s message\n","MSG_OK (on MSG_TRANS_START_V2)");
            *errcode = 61 ;
            /*
            ** Kill all child 
            */
            bbftp_clean_child() ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        msg = (struct message *)sendbuffer ;
        code = msg->code ;
        if ( code == MSG_BAD || code == MSG_BAD_NO_RETRY ) {
            /*
            ** The transfer has failed so ...
            */
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            if (debug) printmessage(stdout,CASE_NORMAL,0,timestamp,"BAD message length = %d\n",msglen) ;
            if ( ( buffer = (char *) malloc (msglen+1) ) == NULL ) {
                printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","buffer 4 (bbftp_get)",strerror(errno));
                *errcode = 35 ;
                pidfree = mychildren ;
                for ( i=0 ; i<requestedstreamnumber ; i++) {
                    if (debug) printf("Killing child %d\n",*pidfree) ; 
                    kill(*pidfree,SIGKILL) ;
                    *pidfree++ = 0 ;
                }
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                return BB_RET_CONN_BROKEN ; /* restart connection */
            }
            if ( readmessage(incontrolsock,buffer,msglen,recvcontrolto,0) < 0 ) {
                printmessage(stderr,CASE_ERROR,67,timestamp,"Error reading data for %s message\n","MSG_BAD (on MSG_TRANS_START_V2)");
                *errcode = 67 ;
                /*
                ** Kill all child 
                */
                bbftp_clean_child() ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                free(buffer) ;
                return BB_RET_CONN_BROKEN ; /* restart connection */
            } else {
                /*
                ** Kill all child 
                */
                bbftp_clean_child() ;
                buffer[msglen] = '\0' ;
                 printmessage(stderr,CASE_ERROR,100,timestamp,"%s\n",buffer) ;
                if (verbose) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< %s\n",buffer) ;
                bbftp_free_all_var() ;
                free(buffer) ;
                *errcode = 100 ;
                if ( code == MSG_BAD) {
                    return BB_RET_ERROR;
                } else {
                    return BB_RET_FT_NR;
                }
            }
        } else if (msg->code == MSG_OK ) {
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            discardmessage(incontrolsock,msglen,recvcontrolto,0) ;
            if ( verbose ) printmessage(stdout,CASE_NORMAL,0,timestamp,"<< OK\n") ;
            (void) gettimeofday(&tend, (struct timezone *)0);
            tdiff.tv_sec = tend.tv_sec - tstart.tv_sec ;
            tdiff.tv_usec = tend.tv_usec - tstart.tv_usec;
            if (tdiff.tv_usec < 0) tdiff.tv_sec--, tdiff.tv_usec += 1000000;
            s = tdiff.tv_sec + (tdiff.tv_usec / 1000000.);
#define nz(x)   ((x) == 0 ? 1 : (x))
            bs = filesize / nz(s);
            if (!simulation_mode) {
                if ( verbose ) printmessage(stdout,CASE_NORMAL,0,timestamp,"%" LONG_LONG_FORMAT" bytes send in %.3g secs (%.3g Kbytes/sec or %.3g Mbits/s)\n", filesize, s, bs / 1024.0,(8.0*bs) / (1024.0 * 1024.0));
            }
            if ( statoutput ) {
                printmessage(stdout,CASE_NORMAL,0,0,"put %" LONG_LONG_FORMAT" %.3f %d %d %d %d %s\n"
                            , filesize, s, buffersizeperstream, sendwinsize, recvwinsize, requestedstreamnumber
                            , ((transferoption & TROPT_GZIP ) == TROPT_GZIP) ? "gzip" : "nogzip" );
            }
            /*
            ** Kill all child 
            */
            bbftp_clean_child() ;
            bbftp_free_all_var() ;
            return BB_RET_OK ;
        } else if ( msg->code == MSG_INFO ) {
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            if ( ( buffer = (char *) malloc (msglen+1) ) == NULL ) {
                printmessage(stderr,CASE_ERROR,35,timestamp,"Error allocating memory for %s : %s\n","buffer 5 (bbftp_get)",strerror(errno));
                *errcode = 35 ;
                bbftp_clean_child() ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                return BB_RET_CONN_BROKEN ; /* restart connection */
            }
            if ( readmessage(incontrolsock,buffer,msglen,recvcontrolto,0) < 0 ) {
                printmessage(stderr,CASE_ERROR,67,timestamp,"Error reading data for %s message\n","MSG_INFO (on MSG_TRANS_START_V2)");
                *errcode = 67 ;
                /*
                ** Kill all child 
                */
                bbftp_clean_child() ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                free(buffer) ;
                return BB_RET_CONN_BROKEN ; /* restart connection */
            }
            buffer[msglen] = '\0' ;
            if (warning) printmessage(stderr,CASE_WARNING,100,timestamp,"%s\n",buffer);
            free(buffer) ;
            goto waitcontrol ;
        } else {
            /*
            ** Receive unkwown message so something is
            ** going wrong. close the control socket
            ** and restart
            */
            printmessage(stderr,CASE_ERROR,62,timestamp,"Unknown message while waiting for %s message\n","MSG_OK (on MSG_TRANS_START_V2)");
            *errcode = 62 ;
            /*
            ** Kill all child 
            */
            bbftp_clean_child() ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ;
        }
    }
}
