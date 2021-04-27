/*
 * bbftpc/bbftp_get.c
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

 bbftp_get.c v 2.0.0 2001/03/13 - Creation of the routine 
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
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif
#include <netinet/in.h>
#include <client.h>
#include <client_proto.h>
#include <common.h>
#include <structures.h>

#include "_bbftp.h"

#ifndef HAVE_NTOHLL
my64_t ntohll(my64_t v) ;
#endif

int bbftp_get(char *remotefilename,int  *errcode) 
{

    int     retcode ;
    char    sendbuffer[STORMESSLEN_V2] ;
    struct  message *msg ;
    struct  mess_store_v2 *msg_store_v2 ;
    char    logmessage[1024] ;
    fd_set  selectmask ; /* Select mask */
    int     nfds ; /* Max number of file descriptor */
    struct  timeval wait_timer;
    char    *buffer ;
    int     msglen ;
    int     code ;
    int     i,k ;
    int     *pidfree = NULL ;
    int     *mysockfree = NULL ;
    int     *porttosend = NULL ;
    int     *portfree = NULL ;
    struct  timeval  tstart, tend ,tdiff;
    float   s, bs;
    int     status ;
    pid_t   pid ;

   memset (sendbuffer, 0, sizeof(sendbuffer));

    if ( BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, ">> COMMAND : get %s %s\n",remotefilename,BBftp_Curfilename) ;
    /*
    ** Check if file is correct 
    */
    if ( (retcode = bbftp_storecheckfile(BBftp_Realfilename,logmessage,errcode)) < 0 ) {
        printmessage(stderr,CASE_ERROR,*errcode, "%s\n",logmessage);
        bbftp_free_all_var() ;
        return  BB_RET_FT_NR ;
    } else if ( retcode > 0 ) {
        printmessage(stderr,CASE_ERROR,*errcode, "%s\n",logmessage);
        bbftp_free_all_var() ;
        return BB_RET_ERROR ;
    }
    msg = (struct message *)sendbuffer ;
    msg->code = MSG_RETR_V2 ;
#ifndef WORDS_BIGENDIAN
    msg->msglen = ntohl(STORMESSLEN_V2) ;
#else
    msg->msglen = STORMESSLEN_V2 ;
#endif
    if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
        /*
        ** We were not able to send the minimum message so
        ** we are going to close the control socket and to 
        ** tell the calling program to restart a connection
        */
        printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_RETR_V2");
        *errcode = 64 ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
    }
    msg_store_v2 = (struct mess_store_v2 *)sendbuffer ;
    /*
    ** Set up the parameters
    */
    if ( BBftp_Debug ) printmessage(stdout,CASE_NORMAL,0, "Requested stream number = %d\n",BBftp_Nbport) ;
    msg_store_v2->transferoption = BBftp_Transferoption ;
#ifndef WORDS_BIGENDIAN
    msg_store_v2->filemode             =  0 ;
    sprintf(msg_store_v2->lastaccess,"%s","") ;
    sprintf(msg_store_v2->lastmodif,"%s","") ;
    msg_store_v2->sendwinsize          = ntohl(BBftp_Sendwinsize) ;
    msg_store_v2->recvwinsize          = ntohl(BBftp_Recvwinsize) ;
    msg_store_v2->buffersizeperstream  = ntohl(BBftp_Buffersizeperstream) ;
    msg_store_v2->nbstream             = ntohl(BBftp_Nbport) ;
    msg_store_v2->filesize             = 0 ;
#else
    msg_store_v2->filemode             =  0 ;
    sprintf(msg_store_v2->lastaccess,"%s","") ;
    sprintf(msg_store_v2->lastmodif,"%s","") ;
    msg_store_v2->sendwinsize          = BBftp_Sendwinsize ;
    msg_store_v2->recvwinsize          = BBftp_Recvwinsize ;
    msg_store_v2->buffersizeperstream  = BBftp_Buffersizeperstream ;
    msg_store_v2->nbstream             = BBftp_Nbport;
    msg_store_v2->filesize             = 0 ;
#endif
    if ( writemessage(BBftp_Outcontrolsock,sendbuffer,STORMESSLEN_V2,BBftp_Sendcontrolto) < 0 ) {
        /*
        ** We were not able to send the minimum message so
        ** we are going to close the control socket and to 
        ** tell the calling program to restart a connection
        */
        printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_RETR_V2 (data)");
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
    if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
        printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_FILENAME");
        *errcode = 64 ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
    }
    if ( writemessage(BBftp_Outcontrolsock,remotefilename,strlen(remotefilename),BBftp_Sendcontrolto) < 0 ) {
        printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_FILENAME (filename)");
        *errcode = 64 ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
    }
    
    /*
    ** At this stage we are waiting for the file length and
    ** characteristics
    */
    
waitcontrol1:
    nfds = sysconf(_SC_OPEN_MAX) ;
   (void) nfds;
    FD_ZERO(&selectmask) ;
    FD_SET(BBftp_Incontrolsock,&selectmask) ;
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
            printmessage(stderr,CASE_ERROR,66, "Error select on control connection : %s\n",strerror(errno));
            *errcode = 66 ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ;
        } else {
            /*
            ** Interrupted by a signal
            */
            FD_ZERO(&selectmask) ;
            FD_SET(BBftp_Incontrolsock,&selectmask) ;
            goto waitcontrol1 ;
        }
    } else if ( retcode == 0 ) {
        goto waitcontrol1 ;
    } else {
        /*
        ** read the message
        */
        if ( readmessage(BBftp_Incontrolsock,sendbuffer,MINMESSLEN,BBftp_Recvcontrolto) < 0 ) {
            printmessage(stderr,CASE_ERROR,61, "Error waiting %s message\n","MSG_OK (on MSG_RETR_V2)");
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
                printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","buffer 1 (bbftp_get)",strerror(errno)) ;
                *errcode = 35 ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                return BB_RET_CONN_BROKEN ;
            }
            if ( readmessage(BBftp_Incontrolsock,buffer,msglen,BBftp_Recvcontrolto) < 0 ) {
                printmessage(stderr,CASE_ERROR,67, "Error reading data for %s message\n","MSG_BAD (on MSG_RETR_V2)");
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
                 printmessage(stderr,CASE_ERROR,100, "%s\n",buffer) ;
                if (BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< %s\n",buffer) ;
                free(buffer) ;
                *errcode = 100 ;
                if ( code == MSG_BAD ) {
                    bbftp_free_all_var() ;
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
        } else if (msg->code == MSG_TRANS_OK_V2 || msg->code == MSG_TRANS_OK_V3 ) {
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            if ( (buffer = (char *) malloc(msglen+1) ) == NULL) {
                printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","buffer 2 (bbftp_get)",strerror(errno)) ;
                *errcode = 35 ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                return BB_RET_CONN_BROKEN ;
            }
            if ( readmessage(BBftp_Incontrolsock,buffer,msglen,BBftp_Recvcontrolto) < 0 ) {
                printmessage(stderr,CASE_ERROR,67, "Error reading data for %s message\n","MSG_TRANS_OK_V2");
                *errcode = 67 ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                free(buffer) ;
                return BB_RET_CONN_BROKEN ;
            }
            msg_store_v2 = (struct mess_store_v2 *)buffer ;
#ifndef WORDS_BIGENDIAN
            BBftp_Filemode                = ntohl(msg_store_v2->filemode) ;
            strncpy(BBftp_Lastaccess,msg_store_v2->lastaccess,8) ;
            BBftp_Lastaccess[8] = '\0' ;
            strncpy(BBftp_Lastmodif,msg_store_v2->lastmodif,8) ;
            BBftp_Lastmodif[8] = '\0' ;
            BBftp_Requestedstreamnumber   = ntohl(msg_store_v2->nbstream) ;
            BBftp_Filesize                = ntohll(msg_store_v2->filesize) ;
#else
            BBftp_Filemode                = msg_store_v2->filemode ;
            strncpy(BBftp_Lastaccess,msg_store_v2->lastaccess,8) ;
            BBftp_Lastaccess[8] = '\0' ;
            strncpy(BBftp_Lastmodif,msg_store_v2->lastmodif,8) ;
            BBftp_Lastmodif[8] = '\0' ;
            BBftp_Requestedstreamnumber   = msg_store_v2->nbstream ;
            BBftp_Filesize                = msg_store_v2->filesize ;
#endif
	    if (BBftp_Filesize != 0) {
            if (msg->code == MSG_TRANS_OK_V3 ) { /* PASSIVE MODE: get the ports */
                if ( (BBftp_Myports = (int *) malloc (BBftp_Requestedstreamnumber*sizeof(int)) ) == NULL ) {
                    printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","ports buffer",strerror(errno)) ;
                    *errcode = 35 ;
                    bbftp_free_all_var() ;
                    bbftp_close_control() ;
                    free(buffer);
                    return BB_RET_CONN_BROKEN ;
                }
                if ( readmessage(BBftp_Incontrolsock,(char *) BBftp_Myports,BBftp_Requestedstreamnumber*sizeof(int),BBftp_Recvcontrolto) < 0 ) {
                    printmessage(stderr,CASE_ERROR,68, "Error reading ports for %s message\n","MSG_TRANS_OK_V3");
                    *errcode = 68 ;
                    bbftp_free_all_var() ;
                    bbftp_close_control() ;
                    free(buffer) ;
                    return BB_RET_CONN_BROKEN ;
                } else {
                    portfree = BBftp_Myports ;
                    for (i=0 ; i < BBftp_Requestedstreamnumber ; i++ ) {
#ifndef WORDS_BIGENDIAN
                        *portfree = ntohl(*portfree) ;
#endif
                        if ( BBftp_Debug ) printmessage(stdout,CASE_NORMAL,0, "Reading port:%d\n",*portfree);
                        portfree++ ;
                    }
                }
            }
	    }
        } else {
            /*
            ** Receive unkwown message so something is
            ** going wrong. close the control socket
            ** and restart
            */
            printmessage(stderr,CASE_ERROR,62, "Unknown message while waiting for %s message\n","MSG_TRANS_OK_V2");
            *errcode = 62 ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ;
        } 
    }
    if ( BBftp_Debug ) printmessage(stdout,CASE_NORMAL,0, "Accepted stream number = %d\n",BBftp_Requestedstreamnumber) ;
    if ( (BBftp_Transferoption & TROPT_GZIP ) == TROPT_GZIP 
        && (msg_store_v2->transferoption & TROPT_GZIP) != TROPT_GZIP ) {
        if (BBftp_Warning) printmessage(stderr,CASE_WARNING,27, "The server cannot handle compression: option ignored\n");
        BBftp_Transferoption = BBftp_Transferoption & ~TROPT_GZIP ;
    }
    if ( (retcode = bbftp_storecreatefile(BBftp_Realfilename,logmessage,errcode)) < 0) {
        /*
        ** Error creating file (fatal error)
        */
        printmessage(stderr,CASE_ERROR,*errcode, "%s\n",logmessage);
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
            printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_free_all_var() ;
            free(buffer) ;
            bbftp_close_control() ;
            return BB_RET_FT_NR_CONN_BROKEN ; /* restart connection */
        }
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_FT_NR ;
    } else if ( retcode > 0 ) {
        /*
        ** Error creating file (non fatal error)
        */
        printmessage(stderr,CASE_ERROR,*errcode, "%s\n",logmessage);
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
            printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_free_all_var() ;
            free(buffer) ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
    }
    BBftp_State = 1 ;
    /*
    ** File creation successfull
    */
    if ( BBftp_Filesize == 0 ) {
        if ( (BBftp_Transferoption & TROPT_ACC ) == TROPT_ACC ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
            if ( (BBftp_Transferoption & TROPT_RFIO_O ) != TROPT_RFIO_O ) {
	       struct  utimbuf ftime ;
	        sscanf(BBftp_Lastaccess,"%08x",&ftime.actime) ;
                sscanf(BBftp_Lastmodif,"%08x",&ftime.modtime) ;
                if ( utime(BBftp_Realfilename,&ftime) < 0 ) {
                    printmessage(stderr,CASE_ERROR,84, "Error changing access time : %s\n",strerror(errno));
                    *errcode = 84 ;
                    unlink(BBftp_Realfilename) ;
                    bbftp_clean_child() ;
                    bbftp_free_all_var() ;
                    return BB_RET_ERROR;
                }
            } else {
                /*
                ** imcompatible options : do nothing
                */
            }
#endif
        }
        if ( (BBftp_Transferoption & TROPT_MODE ) == TROPT_MODE ) {
            if ( bbftp_storechmod(BBftp_Realfilename,BBftp_Filemode,logmessage,errcode) < 0 ) {
                printmessage(stderr,CASE_ERROR,*errcode, "%s\n",logmessage);
                bbftp_storeunlink(BBftp_Realfilename) ;
                bbftp_clean_child() ;
                bbftp_free_all_var() ;
                return BB_RET_ERROR;
            }
        }
        if ( (BBftp_Transferoption & TROPT_TMP ) == TROPT_TMP ) {
#ifdef CASTOR
            if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
                /*
                ** Do nothing
                */
            } else {
                if ( bbftp_storerename(BBftp_Realfilename,BBftp_Curfilename,logmessage,errcode) < 0 ) {
                    printmessage(stderr,CASE_ERROR,*errcode, "%s\n",logmessage);
                    bbftp_storeunlink(BBftp_Realfilename) ;
                    msg = (struct message *)sendbuffer ;
                    msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
                    msg->msglen = ntohl(0) ;
#else
                    msg->msglen = 0;
#endif
                    if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
                        printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_ABORT");
                        *errcode = 64 ;
                        bbftp_free_all_var() ;
                        free(buffer) ;
                        bbftp_close_control() ;
                        return BB_RET_CONN_BROKEN ; /* restart connection */
                    }
                    bbftp_free_all_var() ;
                    free(buffer) ;
                    return BB_RET_ERROR ;
                }
            }
#else 
            if ( bbftp_storerename(BBftp_Realfilename,BBftp_Curfilename,logmessage,errcode) < 0 ) {
                printmessage(stderr,CASE_ERROR,*errcode, "%s\n",logmessage);
                bbftp_storeunlink(BBftp_Realfilename) ;
                msg = (struct message *)sendbuffer ;
                msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
                msg->msglen = ntohl(0) ;
#else
                msg->msglen = 0;
#endif
                if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
                    printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_ABORT");
                    *errcode = 64 ;
                    bbftp_free_all_var() ;
                    free(buffer) ;
                    bbftp_close_control() ;
                    return BB_RET_CONN_BROKEN ; /* restart connection */
                }
                bbftp_free_all_var() ;
                free(buffer) ;
                return BB_RET_ERROR ;
            }
#endif
        }
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_CREATE_ZERO ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
            printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_CREATE_ZERO");
            *errcode = 64 ;
            bbftp_free_all_var() ;
            free(buffer) ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        if (BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_OK ;
    }
    free(buffer) ;
    /*
    ** Reserving memeory to send the port numbers
	** Not used in passive mode but necessary to avoid freeing something NULL later
    */
    if ( (buffer = (char *) malloc (BBftp_Requestedstreamnumber*sizeof(int))) == NULL) {
        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","buffer 3 (bbftp_get)",strerror(errno));
        *errcode = 35 ;
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
            printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_storeunlink(BBftp_Realfilename) ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_storeunlink(BBftp_Realfilename) ;
        bbftp_free_all_var() ;
        return BB_RET_ERROR ;
    }
    /*
    ** We are reserving the memory for the children pid
    */
    if ( (BBftp_Mychildren = (int *) malloc (BBftp_Requestedstreamnumber*sizeof(int))) == NULL) {
        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Mychildren (bbftp_get)",strerror(errno));
        *errcode = 35 ;
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
            printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_storeunlink(BBftp_Realfilename) ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            free(buffer) ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_storeunlink(BBftp_Realfilename) ;
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
    }
    /*
    ** Setting BBftp_Mychildren to 0
    */
    pidfree = BBftp_Mychildren ;
    for (i=1; i<=BBftp_Requestedstreamnumber; i++ ) {
        *pidfree++ = 0 ;
    }
    /*
    ** We are reserving the memory for the readbuffer
    */
    if ( (BBftp_Readbuffer = (char *) malloc (BBftp_Buffersizeperstream*1024)) == NULL) {
        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Readbuffer (bbftp_get)",strerror(errno));
        *errcode = 35 ;
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
            printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_storeunlink(BBftp_Realfilename) ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            free(buffer) ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_storeunlink(BBftp_Realfilename) ;
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
    }
    /*
    ** We are reserving the memory for the compression buffer 
    ** if needed
    */
#ifdef WITH_GZIP
    if ( (BBftp_Transferoption & TROPT_GZIP ) == TROPT_GZIP ) {
        if ( (BBftp_Compbuffer = (char *) malloc (BBftp_Buffersizeperstream*1024)) == NULL) {
            printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Compbuffer (bbftp_get)",strerror(errno));
            *errcode = 35 ;
            msg = (struct message *)sendbuffer ;
            msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
            msg->msglen = ntohl(0) ;
#else
            msg->msglen = 0;
#endif
            if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
                printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_ABORT");
                *errcode = 64 ;
                bbftp_storeunlink(BBftp_Realfilename) ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                free(buffer) ;
                return BB_RET_CONN_BROKEN ; /* restart connection */
            }
            bbftp_storeunlink(BBftp_Realfilename) ;
            bbftp_free_all_var() ;
            free(buffer) ;
            return BB_RET_ERROR ;
        }
    }
#endif
    /*
    ** Now getting space for the port
    */
    if ( BBftp_Protocol == 2 ) { /* ACTIVE MODE */
      if ( (BBftp_Myports = (int *) malloc (BBftp_Requestedstreamnumber*sizeof(int)) ) == NULL ) {
        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Myports (bbftp_get)",strerror(errno));
        *errcode = 35 ;
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
            printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_free_all_var() ;
            bbftp_storeunlink(BBftp_Realfilename) ;
            bbftp_close_control() ;
            free(buffer) ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_storeunlink(BBftp_Realfilename) ;
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
	  }
    }
    if ( (BBftp_Mysockets = (int *) malloc (BBftp_Requestedstreamnumber*sizeof(int)) ) == NULL ) {
        printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","BBftp_Mysockets (bbftp_get)",strerror(errno));
        *errcode = 35 ;
        msg = (struct message *)sendbuffer ;
        msg->code = MSG_ABORT ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(0) ;
#else
        msg->msglen = 0;
#endif
        if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
            printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_storeunlink(BBftp_Realfilename) ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            free(buffer) ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_storeunlink(BBftp_Realfilename) ;
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
    }
/* BUG AIX 5.2 structure mysockets : bad initialisation */
      mysockfree = BBftp_Mysockets ;
      for ( i=0 ; i< BBftp_Requestedstreamnumber ; i++ ) {
        *mysockfree++ = 0 ;
      }
    if (BBftp_Protocol == 2) { /* ACTIVE MODE */
      if ( getdatasock(BBftp_Requestedstreamnumber,errcode) < 0 ) {
        /*
        ** In case of error getdatasock does not clean the
        ** structure and does not close the sockets, it is
        ** the calling program's work
        */ 
        mysockfree = BBftp_Mysockets ;
        for ( i=0 ; i< BBftp_Requestedstreamnumber ; i++ ) {
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
        if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
            printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_ABORT");
            *errcode = 64 ;
            bbftp_storeunlink(BBftp_Realfilename) ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            free(buffer) ;
            return BB_RET_CONN_BROKEN ; /* restart connection */
        }
        bbftp_storeunlink(BBftp_Realfilename) ;
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
      }
      /*
      ** Now we are going to start the children
      */
      if ( (retcode = bbftp_storetransferfile(BBftp_Realfilename,logmessage,errcode)) < 0 ) {
        /*
        ** At this point :  all children have been killed but sockets are not closed
        **                  file is deleted
        */
        mysockfree = BBftp_Mysockets ;
        for ( i=0 ; i< BBftp_Requestedstreamnumber ; i++ ) {
            if ( *mysockfree > 0 ) close(*mysockfree) ;
            mysockfree++ ;
        }
        printmessage(stderr,CASE_ERROR,*errcode, "%s",logmessage);
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
      }
      /*
      ** Close all socket only needed by children
      */
      mysockfree = BBftp_Mysockets ;
      for ( i=0 ; i< BBftp_Requestedstreamnumber ; i++ ) {
        if ( *mysockfree > 0 ) close(*mysockfree) ;
        mysockfree++ ;
      }
      /*
      ** All children are correctly started so send the
      ** START message
      */ 
      msg = (struct message *)sendbuffer ;
      if (BBftp_Simulation_Mode) {
        msg->code = MSG_TRANS_SIMUL ;
      } else {
        msg->code = MSG_TRANS_START_V2 ;
      }
#ifndef WORDS_BIGENDIAN
      msg->msglen = ntohl(BBftp_Requestedstreamnumber*sizeof(int)) ;
#else
      msg->msglen = BBftp_Requestedstreamnumber*sizeof(int) ;
#endif
      if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
        /*
        ** We were not able to send the minimum message so
        ** we are going to close the control socket and to 
        ** tell the calling program to restart a connection
        */
        printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_TRANS_START_V2");
        *errcode = 64 ;
        /*
        ** First kill all child
        */
        bbftp_clean_child() ;
        bbftp_storeunlink(BBftp_Realfilename) ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        free(buffer) ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
      }
      /* 
      ** Second part
      */
      portfree = (int *)buffer ;
      porttosend = BBftp_Myports ;
      for (i=0 ; i<BBftp_Requestedstreamnumber  ; i++) {
#ifndef WORDS_BIGENDIAN
        *portfree++  = ntohl(ntohs(*porttosend++)) ;
#else
        *portfree++ = *porttosend++ ;
#endif
      }
      if ( writemessage(BBftp_Outcontrolsock,buffer,BBftp_Requestedstreamnumber*sizeof(int),BBftp_Sendcontrolto) < 0) {
        printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_TRANS_START_V2 (ports)");
        *errcode = 64 ;
        /*
        ** First kill all child
        */
        bbftp_clean_child() ;
        bbftp_storeunlink(BBftp_Realfilename) ;
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
      if ( (retcode = bbftp_storetransferfile(BBftp_Realfilename,logmessage,errcode)) < 0 ) {
        /*
        ** At this point :  all children have been killed but sockets are not closed
        **                  file is deleted
        */
        mysockfree = BBftp_Mysockets ;
        for ( i=0 ; i< BBftp_Requestedstreamnumber ; i++ ) {
            if ( *mysockfree > 0 ) close(*mysockfree) ;
            mysockfree++ ;
        }
        printmessage(stderr,CASE_ERROR,*errcode, "%s",logmessage);
        bbftp_free_all_var() ;
        free(buffer) ;
        return BB_RET_ERROR ;
      }
      /*
      ** Close all socket only needed by children
      */
      mysockfree = BBftp_Mysockets ;
      for ( i=0 ; i< BBftp_Requestedstreamnumber ; i++ ) {
        if ( *mysockfree > 0 ) close(*mysockfree) ;
        mysockfree++ ;
      }
      /*
      ** All children are correctly started so send the
      ** START message
      */ 
      msg = (struct message *)sendbuffer ;
      if (BBftp_Simulation_Mode) {
        msg->code = MSG_TRANS_SIMUL_V3 ;
      } else {
        msg->code = MSG_TRANS_START_V3 ;
      }
#ifndef WORDS_BIGENDIAN
      msg->msglen = ntohl(BBftp_Requestedstreamnumber*sizeof(int)) ;
#else
      msg->msglen = BBftp_Requestedstreamnumber*sizeof(int) ;
#endif
      if ( writemessage(BBftp_Outcontrolsock,sendbuffer,MINMESSLEN,BBftp_Sendcontrolto) < 0 ) {
        /*
        ** We were not able to send the minimum message so
        ** we are going to close the control socket and to 
        ** tell the calling program to restart a connection
        */
        printmessage(stderr,CASE_ERROR,64, "Error sending %s message\n","MSG_TRANS_START_V3");
        *errcode = 64 ;
        /*
        ** First kill all child
        */
        bbftp_clean_child() ;
        bbftp_storeunlink(BBftp_Realfilename) ;
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
    FD_SET(BBftp_Incontrolsock,&selectmask) ;
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
            printmessage(stderr,CASE_ERROR,66, "Error select on control connection : %s\n",strerror(errno) );
            *errcode = 66 ;
            /*
            ** Kill all child 
            */
            bbftp_clean_child() ;
            bbftp_storeunlink(BBftp_Realfilename) ;
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
        pidfree = BBftp_Mychildren ;
        for ( i = 0 ; i<BBftp_Requestedstreamnumber; i++) {
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
        pidfree = BBftp_Mychildren ;
        for ( i = 0 ; i<BBftp_Requestedstreamnumber; i++) {
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
        printmessage(stderr,CASE_ERROR,37, "No more child\n");
        *errcode = 37 ;
        bbftp_storeunlink(BBftp_Realfilename) ;
        bbftp_free_all_var() ;
        bbftp_close_control() ;
        return BB_RET_CONN_BROKEN ; /* restart connection */
    } else {
        /*
        ** read the message
        */
        if ( readmessage(BBftp_Incontrolsock,sendbuffer,MINMESSLEN,BBftp_Recvcontrolto) < 0 ) {
            printmessage(stderr,CASE_ERROR,61, "Error waiting %s message\n","MSG_OK (on MSG_TRANS_START_V2)");
            *errcode = 61 ;
            /*
            ** Kill all child 
            */
            bbftp_clean_child() ;
            bbftp_storeunlink(BBftp_Realfilename) ;
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
            if (BBftp_Debug) printmessage(stdout,CASE_NORMAL,0, "BAD message length = %d\n",msglen) ;
            if ( ( buffer = (char *) malloc (msglen+1) ) == NULL ) {
                printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","buffer 4 (bbftp_get)",strerror(errno));
                *errcode = 35 ;
                pidfree = BBftp_Mychildren ;
                for ( i=0 ; i<BBftp_Requestedstreamnumber ; i++) {
                    if (BBftp_Debug) printf("Killing child %d\n",*pidfree) ; 
                    kill(*pidfree,SIGKILL) ;
                    *pidfree++ = 0 ;
                }
                bbftp_storeunlink(BBftp_Realfilename) ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                return BB_RET_CONN_BROKEN ; /* restart connection */
            }
            if ( readmessage(BBftp_Incontrolsock,buffer,msglen,BBftp_Recvcontrolto) < 0 ) {
                printmessage(stderr,CASE_ERROR,67, "Error reading data for %s message\n","MSG_BAD (on MSG_TRANS_START_V2)");
                *errcode = 67 ;
                /*
                ** Kill all child 
                */
                bbftp_clean_child() ;
                bbftp_storeunlink(BBftp_Realfilename) ;
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
                 printmessage(stderr,CASE_ERROR,100, "%s\n",buffer) ;
                if (BBftp_Verbose) printmessage(stdout,CASE_NORMAL,0, "<< %s\n",buffer) ;
                bbftp_storeunlink(BBftp_Realfilename) ;
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
            if ( bbftp_storeclosecastfile(BBftp_Realfilename,logmessage) < 0 ) {
                bbftp_storeunlink(BBftp_Realfilename) ;
                printmessage(stderr,CASE_ERROR,80, "Error closing file : %s\n",logmessage);
                bbftp_clean_child() ;
                bbftp_free_all_var() ;
                return BB_RET_ERROR;
            }
            if ( (BBftp_Transferoption & TROPT_ACC ) == TROPT_ACC ) {
#if defined(WITH_RFIO) || defined(WITH_RFIO64)
                if ( (BBftp_Transferoption & TROPT_RFIO_O ) != TROPT_RFIO_O ) {
		   struct  utimbuf ftime ;
                    sscanf(BBftp_Lastaccess,"%08x",&ftime.actime) ;
                    sscanf(BBftp_Lastmodif,"%08x",&ftime.modtime) ;
                    if ( utime(BBftp_Realfilename,&ftime) < 0 ) {
                        printmessage(stderr,CASE_ERROR,84, "Error changing access time : %s\n",strerror(errno));
                        *errcode = 84 ;
                        bbftp_storeunlink(BBftp_Realfilename) ;
                        bbftp_clean_child() ;
                        bbftp_free_all_var() ;
                        return BB_RET_ERROR;
                    }
                } else {
                    /*
                    ** imcompatible options : do nothing
                    */
                }
#endif
            }
            if ( (BBftp_Transferoption & TROPT_MODE ) == TROPT_MODE ) {
                if ( bbftp_storechmod(BBftp_Realfilename,BBftp_Filemode,logmessage,errcode) < 0 ) {
                    printmessage(stderr,CASE_ERROR,*errcode, "%s\n",logmessage);
                    bbftp_storeunlink(BBftp_Realfilename) ;
                    bbftp_clean_child() ;
                    bbftp_free_all_var() ;
                    return BB_RET_ERROR;
                }
            }
            if ((BBftp_Transferoption & TROPT_TMP ) == TROPT_TMP ) {
#ifdef CASTOR
                if ( (BBftp_Transferoption & TROPT_RFIO_O) == TROPT_RFIO_O ) {
                    /*
                    ** Do nothing
                    */
                } else {
                    if ( bbftp_storerename(BBftp_Realfilename,BBftp_Curfilename,logmessage,errcode) < 0 ) {
                        printmessage(stderr,CASE_ERROR,*errcode, "%s\n",logmessage);
                        bbftp_storeunlink(BBftp_Realfilename) ;
                        bbftp_clean_child() ;
                        bbftp_free_all_var() ;
                        return BB_RET_ERROR;
                    }
                }
#else 
                if ( bbftp_storerename(BBftp_Realfilename,BBftp_Curfilename,logmessage,errcode) < 0 ) {
                    printmessage(stderr,CASE_ERROR,*errcode, "%s\n",logmessage);
                    bbftp_storeunlink(BBftp_Realfilename) ;
                    bbftp_clean_child() ;
                    bbftp_free_all_var() ;
                    return BB_RET_ERROR;
                }
#endif
            }
            /*
            ** In simulation mode, the file is deleted
            */
            if (BBftp_Simulation_Mode) {
                bbftp_storeunlink(BBftp_Curfilename) ;
            }
#ifndef WORDS_BIGENDIAN
            msglen = ntohl(msg->msglen) ;
#else
            msglen = msg->msglen ;
#endif
            discardmessage(BBftp_Incontrolsock,msglen,BBftp_Recvcontrolto) ;
            if ( BBftp_Verbose ) printmessage(stdout,CASE_NORMAL,0, "<< OK\n") ;
            (void) gettimeofday(&tend, (struct timezone *)0);
            tdiff.tv_sec = tend.tv_sec - tstart.tv_sec ;
            tdiff.tv_usec = tend.tv_usec - tstart.tv_usec;
            if (tdiff.tv_usec < 0) tdiff.tv_sec--, tdiff.tv_usec += 1000000;
            s = tdiff.tv_sec + (tdiff.tv_usec / 1000000.);
#define nz(x)   ((x) == 0 ? 1 : (x))
            bs = BBftp_Filesize / nz(s);
            if (!BBftp_Simulation_Mode) {
                if ( BBftp_Verbose ) printmessage(stdout,CASE_NORMAL,0, "%" LONG_LONG_FORMAT" bytes got in %.3g secs (%.3g Kbytes/sec or %.3g Mbits/s)\n", BBftp_Filesize, s, bs / 1024.0,(8.0*bs) / (1024.0 * 1024.0));
            }
            if ( BBftp_Statoutput ) {
                printstdout("get %" LONG_LONG_FORMAT" %.3f %d %d %d %d %s\n"
                            , BBftp_Filesize, s, BBftp_Buffersizeperstream, BBftp_Sendwinsize, BBftp_Recvwinsize, BBftp_Requestedstreamnumber
                            , ((BBftp_Transferoption & TROPT_GZIP ) == TROPT_GZIP) ? "gzip" : "nogzip" );
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
                printmessage(stderr,CASE_ERROR,35, "Error allocating memory for %s : %s\n","buffer 5 (bbftp_get)",strerror(errno));
                *errcode = 35 ;
                bbftp_clean_child() ;
                bbftp_storeunlink(BBftp_Realfilename) ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                return BB_RET_CONN_BROKEN ; /* restart connection */
            }
            if ( readmessage(BBftp_Incontrolsock,buffer,msglen,BBftp_Recvcontrolto) < 0 ) {
                printmessage(stderr,CASE_ERROR,67, "Error reading data for %s message\n","MSG_INFO (on MSG_TRANS_START_V2)");
                /*
                ** Kill all child 
                */
                *errcode = 67 ;
                bbftp_clean_child() ;
                bbftp_storeunlink(BBftp_Realfilename) ;
                bbftp_free_all_var() ;
                bbftp_close_control() ;
                free(buffer) ;
                return BB_RET_CONN_BROKEN ; /* restart connection */
            }
            buffer[msglen] = '\0' ;
            if (BBftp_Warning) printmessage(stderr,CASE_WARNING,100, "%s\n",buffer);
            free(buffer) ;
            goto waitcontrol ;
        } else {
            /*
            ** Receive unkwown message so something is
            ** going wrong. close the control socket
            ** and restart
            */
            printmessage(stderr,CASE_ERROR,62, "Unknown message while waiting for %s message\n","MSG_OK (on MSG_TRANS_START_V2)");
            *errcode = 62 ;
            /*
            ** Kill all child 
            */
            bbftp_clean_child() ;
            bbftp_storeunlink(BBftp_Realfilename) ;
            bbftp_free_all_var() ;
            bbftp_close_control() ;
            return BB_RET_CONN_BROKEN ;
        }
    }
}
