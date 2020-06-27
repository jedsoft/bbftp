/*
 * bbftpd/bbftpd_readcontrol.c
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

  
 RETURN:
         0  calling program return to main loop
        -1 calling program exit
 
 bbftpd_readcontrol.c   v 2.0.0 2000/12/19  - Routine creation
                        v 2.0.1 2001/04/23  - Correct indentation
                        v 2.0.2 2001/05/04  - Correct return code treatment
                                            - Correct case of file size 0
                        v 2.1.0 2001/05/30  - Correct syslog level
                                            - Change routines name
                        v 2.2.0 2001/11/21  - Remote COS received can be -1
               
*****************************************************************************/
#include <bbftpd.h>

#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <utime.h>
#include <sys/types.h>
#include <unistd.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <netinet/in.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <status.h>
#include <structures.h>
#include <version.h>

extern int  state ;
extern int  incontrolsock ;
extern int  outcontrolsock ;
extern	int	recvcontrolto ;
extern	int	sendcontrolto ;
extern char *curfilename ;
extern char *realfilename ;
extern int  curfilenamelen ;
extern int  transferoption ;
extern int  filemode ;
extern char lastaccess[9] ;
extern char lastmodif[9] ;
extern int  sendwinsize ;        
extern int  recvwinsize ;        
extern int  buffersizeperstream ;
extern int  requestedstreamnumber ;
extern my64_t  filesize ;
extern int  *myports ;
extern int  *mychildren ;
extern int  *mysockets ;
extern int  myumask ;
extern int  mycos ;
extern char *readbuffer ;
extern char *compbuffer ;
extern int  protocolversion ;
extern  char            currentusername[MAXLEN] ;

#ifndef WORDS_BIGENDIAN
# ifndef HAVE_NTOHLL
my64_t    ntohll(my64_t v) ;
# endif
#endif

int bbftpd_readcontrol(int msgcode,int msglen) 
{

    int        retcode ;
    int        i ;
    char    *receive_buffer ;
    int     oldumask, newumask ;
    struct  message *msg ;
    struct  mess_store_v2 *msg_store_v2 ;
    struct  mess_integer *msg_integer ;
    struct  mess_dir *msg_dir ;
    char    logmessage[1024] ;
    int     *portfree ;
    struct utimbuf ftime ;
    int     *porttosend ;    
    /*
    ** Depending on the state the accepted message may be different
    */
#ifndef WORDS_BIGENDIAN
    msglen = ntohl(msglen) ;
#endif
    switch (state) {
        /*
        ** Case S_LOGGED :
        **      Nothing is going on, so message accepted are :
        **
        **         MSG_CHCOS
        **         MSG_CHDIR_V2
        **         MSG_CHUMASK
        **         MSG_CLOSE_CONN
        **         MSG_LIST_V2
        **         MSG_MKDIR_V2
        **         MSG_RETR_V2
        **         MSG_STORE_V2
        **
        */
           case S_LOGGED : {
            switch (msgcode) {
            
                case MSG_CHCOS : {
                    syslog(BBFTPD_DEBUG,"Receiving MSG_CHCOS, msglen = %d",msglen) ;
                    if ( (receive_buffer = (char *) malloc (msglen)) == NULL ) {
                        syslog(BBFTPD_ERR,"Error allocating memory for MSG_CHCOS : %s",strerror(errno)) ;
                        reply(MSG_BAD,"Error allocating memory for MSG_CHCOS") ;
                        return 0 ;
                    }
                    if ( readmessage(incontrolsock,receive_buffer,msglen,recvcontrolto) < 0 ) {
                        syslog(BBFTPD_ERR,"Error reading MSG_CHCOS") ;
                        FREE(receive_buffer) ;
                        return -1 ;
                    }
                    msg_integer = (struct  mess_integer *) receive_buffer ;
#ifndef WORDS_BIGENDIAN
                    mycos = ntohl(msg_integer->myint) ;
#else
                    mycos = msg_integer->myint ;
#endif
                    if (mycos>=0) {
                        syslog(BBFTPD_DEBUG,"Cos set to %d",mycos) ;
                        reply(MSG_OK,"COS set") ;
                    } else {
                        syslog(BBFTPD_DEBUG,"Cos received : %d",mycos) ;
                        reply(MSG_OK,"COS not set") ;
                    }
                    FREE(receive_buffer) ;
                    return 0 ;
               }

                case MSG_CHDIR_V2 : {
                    syslog(BBFTPD_DEBUG,"Receiving MSG_CHDIR_V2, msglen = %d",msglen) ;
                    retcode = bbftpd_cd(incontrolsock,msglen) ;
                    return retcode ;
                }
                
                case MSG_CHUMASK : {
                    syslog(BBFTPD_DEBUG,"Receiving MSG_UMASK ") ;
                    if ( (receive_buffer = (char *) malloc (msglen)) == NULL ) {
                        syslog(BBFTPD_ERR,"Error allocating memory for MSG_UMASK : %s",strerror(errno)) ;
                        reply(MSG_BAD,"Error allocating memory for MSG_UMASK") ;
                        return 0 ;
                    }
                    if ( readmessage(incontrolsock,receive_buffer,msglen,recvcontrolto) < 0 ) {
                        syslog(BBFTPD_ERR,"Error reading MSG_UMASK") ;
                        FREE(receive_buffer) ;
                        return -1 ;
                    }
                    msg_integer = (struct  mess_integer *) receive_buffer ;
#ifndef WORDS_BIGENDIAN
                    myumask = ntohl(msg_integer->myint) ;
#else
                    myumask = msg_integer->myint ;
#endif
                    /*
                    ** We reset the umask first then call twice the umask
                    ** in order to see the real umask set
                    */
                    oldumask = umask(0) ;
                    umask(myumask) ;
                    newumask = umask(myumask) ;
                    sprintf(logmessage,"umask changed from %04o to %04o",oldumask,newumask) ;
                    reply(MSG_OK,logmessage) ;
                    FREE(receive_buffer) ;
                    return 0 ;
                }
 
                case MSG_CLOSE_CONN : {
                    syslog(BBFTPD_DEBUG,"Receiving MSG_CLOSE_CONN, msglen = %d",msglen) ;
                    return -1 ;
                }
                
                
                case MSG_LIST_V2 :{
                   syslog(BBFTPD_DEBUG,"Receiving MSG_LIST_V2 ") ;
                   if ( (receive_buffer = (char *) malloc (msglen+1) ) == NULL ) {
                        syslog(BBFTPD_ERR,"Unable to malloc space for directory name (%d)",msglen) ;
                        reply(MSG_BAD,"Unable to malloc space for directory name") ;
                       return 0 ;
                   }
                  /*
                   ** Read the characteristics of the directory
                   */
                   if ( readmessage(incontrolsock,receive_buffer,msglen,recvcontrolto) < 0 ) {
                       /*
                       ** Error ...
                       */
                        syslog(BBFTPD_ERR,"Error reading directory name") ;
                       FREE(receive_buffer) ;
                       return -1 ;
                   }
                   /*
                   ** buffer contains the directory to create
                   */
                   receive_buffer[msglen] = '\0' ;
                   msg_dir = (struct mess_dir *) receive_buffer ;
                   transferoption  = msg_dir->transferoption ;
                   syslog(BBFTPD_DEBUG,"Pattern = %s",msg_dir->dirname) ;
                   retcode = bbftpd_list(msg_dir->dirname,logmessage) ;
                   FREE(receive_buffer) ;
                   return retcode ;
                }
                
                case MSG_MKDIR_V2 : {
                    syslog(BBFTPD_DEBUG,"Receiving MSG_MKDIR_V2 ") ;
                    retcode = bbftpd_mkdir(incontrolsock,msglen) ;
                    return retcode ;
                }
                
                case MSG_RM : {
                    syslog(BBFTPD_DEBUG,"Receiving MSG_RM ") ;
                    retcode = bbftpd_rm(incontrolsock,msglen) ;
                    return retcode ;
                }
                
		case MSG_STAT : {
                    syslog(BBFTPD_DEBUG,"Receiving MSG_STAT ") ;
                    retcode = bbftpd_stat(incontrolsock,msglen) ;
                    return retcode ;
                }
                    
		case MSG_DF : {
                    syslog(BBFTPD_DEBUG,"Receiving MSG_DF ") ;
                    retcode = bbftpd_statfs(incontrolsock,msglen) ;
                    return retcode ;
                }
                    
                case MSG_RETR_V2 : {
                    syslog(BBFTPD_DEBUG,"Receiving MSG_RETR_V2") ;
                    if ( (receive_buffer = (char *) malloc (msglen)) == NULL ) {
                        syslog(BBFTPD_ERR,"Error allocating memory for MSG_RETR_V2 : %s",strerror(errno)) ;
                        return -1 ;
                    }
                     if ( readmessage(incontrolsock,receive_buffer,msglen,recvcontrolto) < 0 ) {
                        syslog(BBFTPD_ERR,"Error reading MSG_RETR_V2") ;
                        FREE(receive_buffer) ;
                        return -1 ;
                    }
                    msg_store_v2 = (struct  mess_store_v2 *) receive_buffer ;
                    /*
                    ** Set up the transfer parameter, we are going only to store 
                    ** them 
                    */
#ifndef WORDS_BIGENDIAN
                    transferoption          = msg_store_v2->transferoption ;
                    sendwinsize             = ntohl(msg_store_v2->sendwinsize) ;
                    recvwinsize             = ntohl(msg_store_v2->recvwinsize) ;
                    buffersizeperstream     = ntohl(msg_store_v2->buffersizeperstream) ;
                    requestedstreamnumber   = ntohl(msg_store_v2->nbstream) ;
#else
                    transferoption          = msg_store_v2->transferoption ;
                    sendwinsize             = msg_store_v2->sendwinsize ;
                    recvwinsize             = msg_store_v2->recvwinsize ;
                    buffersizeperstream     = msg_store_v2->buffersizeperstream ;
                    requestedstreamnumber   = msg_store_v2->nbstream ;
#endif
                    syslog(BBFTPD_DEBUG,"transferoption         = %d ",transferoption) ;
                    syslog(BBFTPD_DEBUG,"sendwinsize            = %d ",sendwinsize) ;
                    syslog(BBFTPD_DEBUG,"recvwinsize            = %d ",recvwinsize) ;
                    syslog(BBFTPD_DEBUG,"buffersizeperstream    = %d ",buffersizeperstream) ;
                    syslog(BBFTPD_DEBUG,"requestedstreamnumber  = %d ",requestedstreamnumber) ;
                    state = S_WAITING_FILENAME_RETR ;
                    FREE(receive_buffer) ;
                    return 0 ;
                }
                
                case MSG_STORE_V2 :{
                    syslog(BBFTPD_DEBUG,"Receiving MSG_STORE_V2") ;
                    if ( (receive_buffer = (char *) malloc (msglen)) == NULL ) {
                        syslog(BBFTPD_ERR,"Error allocating memory for MSG_STORE_V2 : %s",strerror(errno)) ;
                        return -1 ;
                    }
                     if ( readmessage(incontrolsock,receive_buffer,msglen,recvcontrolto) < 0 ) {
                        syslog(BBFTPD_ERR,"Error reading MSG_STORE_V2") ;
                        FREE(receive_buffer) ;
                        return -1 ;
                    }
                    msg_store_v2 = (struct  mess_store_v2 *) receive_buffer ;
                    /*
                    ** Set up the transfer parameter, we are going only to store 
                    ** them 
                    */
#ifndef WORDS_BIGENDIAN
                    transferoption          = msg_store_v2->transferoption ;
                    filemode                = ntohl(msg_store_v2->filemode) ;
                    strncpy(lastaccess,msg_store_v2->lastaccess,8) ;
                    lastaccess[8] = '\0' ;
                    strncpy(lastmodif,msg_store_v2->lastmodif,8) ;
                    lastmodif[8] = '\0' ;
                    sendwinsize             = ntohl(msg_store_v2->sendwinsize) ;
                    recvwinsize             = ntohl(msg_store_v2->recvwinsize) ;
                    buffersizeperstream     = ntohl(msg_store_v2->buffersizeperstream) ;
                    requestedstreamnumber   = ntohl(msg_store_v2->nbstream) ;
                    filesize                = ntohll(msg_store_v2->filesize) ;
#else
                    transferoption          = msg_store_v2->transferoption ;
                    filemode                = msg_store_v2->filemode ;
                    strncpy(lastaccess,msg_store_v2->lastaccess,8) ;
                    lastaccess[8] = '\0' ;
                    strncpy(lastmodif,msg_store_v2->lastmodif,8) ;
                    lastmodif[8] = '\0' ;
                    sendwinsize             = msg_store_v2->sendwinsize ;
                    recvwinsize             = msg_store_v2->recvwinsize ;
                    buffersizeperstream     = msg_store_v2->buffersizeperstream ;
                    requestedstreamnumber   = msg_store_v2->nbstream ;
                    filesize                = msg_store_v2->filesize ;
#endif
                    syslog(BBFTPD_DEBUG,"transferoption         = %d ",transferoption) ;
                    syslog(BBFTPD_DEBUG,"filemode               = %d ",filemode) ;
                    syslog(BBFTPD_DEBUG,"lastaccess             = %s ",lastaccess) ;
                    syslog(BBFTPD_DEBUG,"lastmodif              = %s ",lastmodif) ;
                    syslog(BBFTPD_DEBUG,"sendwinsize            = %d ",sendwinsize) ;
                    syslog(BBFTPD_DEBUG,"recvwinsize            = %d ",recvwinsize) ;
                    syslog(BBFTPD_DEBUG,"buffersizeperstream    = %d ",buffersizeperstream) ;
                    syslog(BBFTPD_DEBUG,"requestedstreamnumber  = %d ",requestedstreamnumber) ;
                    syslog(BBFTPD_DEBUG,"filesize               = %" LONG_LONG_FORMAT " ",filesize) ;
                    state = S_WAITING_FILENAME_STORE ;
                    FREE(receive_buffer) ;
                    return 0 ;
                }
                
                default :{
                    syslog(BBFTPD_ERR,"Unkown message in logged state %d",msgcode) ;
                    retcode = discardmessage(incontrolsock,msglen,recvcontrolto) ;
                    reply(MSG_BAD_NO_RETRY,"Unkown message in S_LOGGED state") ;
                    return retcode ;
                }
            }
        }
        /*
        ** Case S_WAITING_FILENAME_STORE
        **
        **         MSG_FILENAME
        */
           case S_WAITING_FILENAME_STORE : {
            switch (msgcode) {
                case MSG_FILENAME :{
                    syslog(BBFTPD_DEBUG,"Receiving MSG_FILENAME in S_WAITING_FILENAME_STORE state") ;
                    if ( msglen == 0 ) {
                         syslog(BBFTPD_ERR,"Filename length null") ;
                         return -1 ;
                    }
                    if ( (curfilename = (char *) malloc (msglen+1)) == NULL ) {
                        syslog(BBFTPD_ERR,"Error allocating memory for filename : %s",strerror(errno)) ;
                        return -1 ;
                    }
		    /* tmp name = <name>.bbftp.tmp.<hostname in 10 chars>.<pid in 5 chars> */
                    if ( (realfilename = (char *) malloc (msglen+30)) == NULL ) {
                        syslog(BBFTPD_ERR,"Error allocating memory for realfilename : %s",strerror(errno)) ;
                        free_all_var() ;
                        return -1 ;
                    }
                    if ( readmessage(incontrolsock,curfilename,msglen,recvcontrolto) < 0 ) {
                        syslog(BBFTPD_ERR,"Error reading filename") ;
                        free_all_var() ;
                        return -1 ;
                    }
                    curfilename[msglen] = '\0' ;
                    curfilenamelen = msglen ;
                    syslog(BBFTPD_DEBUG,"Request to store file %s",curfilename) ;
                    if ( (retcode = bbftpd_storecheckoptions(logmessage)) < 0 ) {
                        syslog(BBFTPD_ERR,logmessage) ;
                        reply(MSG_BAD_NO_RETRY,logmessage) ;
                        free_all_var() ;
                        state = S_LOGGED ;
                        return 0 ;
                    }
                    /*
                    ** Options are corrects
                    */
                    if ( (transferoption & TROPT_TMP ) == TROPT_TMP ) {
                        /*
                        ** Caculate tempfile name if TROPT_TMP is set
			** To avoid multiple access to temp file in HPSS
			** file is called .bbftp.tmp.$host.$pid
                        */
			char hostname[10 + 1];
			if (gethostname(hostname, sizeof(hostname)) < 0) {
				sprintf(realfilename,"%s.bbftp.tmp.%d",curfilename,getpid());
			} else {
				hostname[sizeof(hostname) - 1] = '\0';
                        	sprintf(realfilename,"%s.bbftp.tmp.%s.%d",curfilename,hostname,getpid()) ;
			}
                    } else {
                        sprintf(realfilename,"%s",curfilename) ;
                    }
                    /*
                    ** Create the file
                    */
                    if ( (retcode = bbftpd_storecreatefile(realfilename,logmessage)) < 0 ) {
                        syslog(BBFTPD_ERR,logmessage) ;
                        reply(MSG_BAD_NO_RETRY,logmessage) ;
                        free_all_var() ;
                        state = S_LOGGED ;
                        return 0 ;
                    } else if ( retcode > 0 ) {
                        syslog(BBFTPD_ERR,logmessage) ;
                        reply(MSG_BAD,logmessage) ;
                        free_all_var() ;
                        state = S_LOGGED ;
                        return 0 ;
                    } else {
                        if ( filesize == 0 ) {
                            if ((transferoption & TROPT_ACC ) == TROPT_ACC ) {
                                sscanf(lastaccess,"%08x",&ftime.actime) ;
                                sscanf(lastmodif,"%08x",&ftime.modtime) ;
                                if ( bbftpd_storeutime(realfilename,&ftime,logmessage) < 0 ) {
                                    syslog(BBFTPD_ERR,logmessage) ;
                                    bbftpd_storeunlink(realfilename) ;
                                    reply(MSG_BAD,logmessage) ;
                                    free_all_var() ;
                                    state = S_LOGGED ;
                                    return 0 ;
                                }
                           }
                           if ( (transferoption & TROPT_MODE ) == TROPT_MODE ) {
                                if ( bbftpd_storechmod(realfilename,filemode,logmessage) < 0 ) {
                                    syslog(BBFTPD_ERR,logmessage) ;
                                    bbftpd_storeunlink(realfilename) ;
                                    reply(MSG_BAD,logmessage) ;
                                    free_all_var() ;
                                    state = S_LOGGED ;
                                    return 0 ;
                                }
                            }
                            if ( (transferoption & TROPT_TMP ) == TROPT_TMP ) {
                                if ( bbftpd_storerename(realfilename,curfilename,logmessage) < 0 ) {
                                    syslog(BBFTPD_ERR,logmessage) ;
                                    bbftpd_storeunlink(realfilename) ;
                                    reply(MSG_BAD,logmessage) ;
                                    free_all_var() ;
                                    state = S_LOGGED ;
                                    return 0 ;
                                }
                            }
                            reply(MSG_OK,"") ;
                            free_all_var() ;
                            state = S_LOGGED ;
                            return 0 ;
                        } else {
                            /*
                            ** We are reserving the memory for the ports
                            */
                            if ( (myports = (int *) malloc (requestedstreamnumber*sizeof(int))) == NULL ) {
                                bbftpd_storeunlink(realfilename) ;
                                free_all_var() ;
                                syslog(BBFTPD_ERR,"Unable to allocate memory for myports") ;
                                sprintf(logmessage,"Unable to allocate memory for myports") ;
                                reply(MSG_BAD,logmessage) ;
                                state = S_LOGGED ;
                                return 0 ;
                            }
                            /*
                            ** We are reserving the memory for the children pid
                            */
                            if ( (mychildren = (int *) malloc (requestedstreamnumber*sizeof(int))) == NULL) {
                                bbftpd_storeunlink(realfilename) ;
                                free_all_var() ;
                                syslog(BBFTPD_ERR,"Unable to allocate memory for mychildren") ;
                                sprintf(logmessage,"Unable to allocate memory for mychildren") ;
                                reply(MSG_BAD,logmessage) ;
                                state = S_LOGGED ;
                                return 0 ;
                            }
                            /*
                            ** We are reserving the memory for the readbuffer
                            */
                            if ( (readbuffer = (char *) malloc (buffersizeperstream*1024)) == NULL) {
                                bbftpd_storeunlink(realfilename) ;
                                free_all_var() ;
                                syslog(BBFTPD_ERR,"Unable to allocate memory for readbuffer") ;
                                sprintf(logmessage,"Unable to allocate memory for readbuffer") ;
                                reply(MSG_BAD,logmessage) ;
                                state = S_LOGGED ;
                                return 0 ;
                            }
                            /*
                            ** We are reserving the memory for the compression buffer 
                            ** if needed
                            */
                            if ( (transferoption & TROPT_GZIP ) == TROPT_GZIP ) {
                                if ( (compbuffer = (char *) malloc (buffersizeperstream*1024)) == NULL) {
                                    bbftpd_storeunlink(realfilename) ;
                                    free_all_var() ;
                                    syslog(BBFTPD_ERR,"Unable to allocate memory for compbuffer") ;
                                    sprintf(logmessage,"Unable to allocate memory for compbuffer") ;
                                    reply(MSG_BAD,logmessage) ;
                                    state = S_LOGGED ;
                                    return 0 ;
                                }
                            }
                            if ( (receive_buffer = (char *) malloc (STORMESSLEN_V2)) == NULL ) {
                                bbftpd_storeunlink(realfilename) ;
                                syslog(BBFTPD_ERR,"Error allocating memory for MSG_TRANS_OK_V2 : %s",strerror(errno)) ;
                                free_all_var() ;
                                sprintf(logmessage,"Unable to allocate memory for message MSG_TRANS_OK_V2") ;
                                reply(MSG_BAD,logmessage) ;
                                state = S_LOGGED ;
                                return 0 ;
                            }
                            msg = (struct message *) receive_buffer ;
                            if ( protocolversion == 2) { /* ACTIVE MODE */
                              msg->code = MSG_TRANS_OK_V2 ;
                            } else { /* PASSIVE MODE */
                              msg->code = MSG_TRANS_OK_V3 ;
                            }
#ifndef WORDS_BIGENDIAN
                            msg->msglen = ntohl(STORMESSLEN_V2) ;
#else
                            msg->msglen = STORMESSLEN_V2 ;
#endif
                            if ( writemessage(outcontrolsock,receive_buffer,MINMESSLEN,sendcontrolto) < 0 ) {
                                syslog(BBFTPD_ERR,"Error wtiting MSG_TRANS_OK_V2 first part") ;
                                bbftpd_storeunlink(realfilename) ;
                                free_all_var() ;
                                FREE(receive_buffer) ;
                                return -1 ;
                            }
                            msg_store_v2 = (struct  mess_store_v2 *) receive_buffer ;
                            /*
                            ** Set up the transfer parameter
                            */
#ifndef WORDS_BIGENDIAN
                            msg_store_v2->transferoption       = transferoption ;
                            msg_store_v2->filemode             = ntohl(filemode) ;
                            strncpy(msg_store_v2->lastaccess,lastaccess,8) ;
                            msg_store_v2->lastaccess[8] = '0' ;
                            strncpy(msg_store_v2->lastmodif,lastmodif,8) ;
                            msg_store_v2->lastmodif[8] = '0' ;
                            msg_store_v2->sendwinsize          = ntohl(sendwinsize) ;
                            msg_store_v2->recvwinsize          = ntohl(recvwinsize) ;
                            msg_store_v2->buffersizeperstream  = ntohl(buffersizeperstream) ;
                            msg_store_v2->nbstream             = ntohl(requestedstreamnumber) ;
                            msg_store_v2->filesize             = ntohll(filesize) ;
#else
                            msg_store_v2->transferoption       = transferoption  ;       
                            msg_store_v2->filemode             =  filemode;
                            strncpy(msg_store_v2->lastaccess,lastaccess,8) ;
                            msg_store_v2->lastaccess[8] = '0' ;
                            strncpy(msg_store_v2->lastmodif,lastmodif,8) ;
                            msg_store_v2->lastmodif[8] = '0' ;
                            msg_store_v2->sendwinsize          = sendwinsize ;
                            msg_store_v2->recvwinsize          = recvwinsize ;
                            msg_store_v2->buffersizeperstream  = buffersizeperstream ;
                            msg_store_v2->nbstream             = requestedstreamnumber;
                            msg_store_v2->filesize             = filesize ;
#endif
                            if ( writemessage(outcontrolsock,receive_buffer,STORMESSLEN_V2,sendcontrolto) < 0 ) {
                                syslog(BBFTPD_ERR,"Error wtiting MSG_TRANS_OK_V2 second part") ;
                                bbftpd_storeunlink(realfilename) ;
                                free_all_var() ;
                                FREE(receive_buffer) ;
                                return -1 ;
                            }
                            /* PASSIVE MODE: send ports */
                            if ( protocolversion >= 3 ) {
                              /*
                              ** We are reserving the memory for the ports
                              */
                              if ( (mysockets = (int *) malloc (requestedstreamnumber*sizeof(int))) == NULL ) {
                                syslog(BBFTPD_ERR,"Error allocating space for sockets") ;
                                bbftpd_storeunlink(realfilename) ;
                                free_all_var() ;
                                FREE(receive_buffer) ;
                                return -1 ;
                              }
                              if ( bbftpd_getdatasock(requestedstreamnumber) < 0 ) {
                                syslog(BBFTPD_ERR,"Error creating data sockets") ;
                                bbftpd_storeunlink(realfilename) ;
                                free_all_var() ;
                                FREE(receive_buffer) ;
                                return -1 ;
                              }
                              portfree = (int *)receive_buffer ;
                              porttosend = myports ;
                              for (i=0 ; i<requestedstreamnumber  ; i++) {
#ifndef WORDS_BIGENDIAN
                                *portfree++  = ntohl(ntohs(*porttosend++)) ;
#else
                                *portfree++ = *porttosend++ ;
#endif
                              }
/*          syslog(BBFTPD_INFO,"Port=%d,socket=%d\n",*myports,*mysockets) ;*/
                              if ( writemessage(outcontrolsock,receive_buffer,requestedstreamnumber*sizeof(int),sendcontrolto) < 0) {
                                syslog(BBFTPD_ERR,"Error writing MSG_TRANS_OK_V3 (ports)") ;
                                bbftpd_storeunlink(realfilename) ;
                                free_all_var() ;
                                FREE(receive_buffer) ;
                                return -1 ;
                              }
                            } /* END PASSIVE MODE */
                            state = S_WAITING_STORE_START ;
                            FREE(receive_buffer) ;
                            return 0 ;
                        }
                    }
                }
                default :{
                    syslog(BBFTPD_ERR,"Unkown message in S_WAITING_FILENAME_STORE state : %d",msgcode) ;
                    return -1 ;
                }
            }
        }
        /*
        ** Case S_WAITING_FILENAME_RETR
        **
        **         MSG_FILENAME
        */
           case S_WAITING_FILENAME_RETR : {
            switch (msgcode) {
                case MSG_FILENAME :{
                    syslog(BBFTPD_DEBUG,"Receiving MSG_FILENAME in S_WAITING_FILENAME_RETR state") ;
                    if ( msglen == 0 ) {
                         syslog(BBFTPD_ERR,"Filename length null") ;
                         return -1 ;
                    }
                    if ( (curfilename = (char *) malloc (msglen+1)) == NULL ) {
                        syslog(BBFTPD_ERR,"Error allocating memory for filename : %s",strerror(errno)) ;
                        return -1 ;
                    }
                    if ( readmessage(incontrolsock,curfilename,msglen,recvcontrolto) < 0 ) {
                        syslog(BBFTPD_ERR,"Error reading filename") ;
                        free_all_var() ;
                        return -1 ;
                    }
                    curfilename[msglen] = '\0' ;
                    curfilenamelen = msglen ;
                    syslog(BBFTPD_DEBUG,"Request to retreive file %s",curfilename) ;
                    /*
                    ** Create the file
                    */
                    if ( (retcode = bbftpd_retrcheckfile(curfilename,logmessage)) < 0 ) {
                        syslog(BBFTPD_ERR,logmessage) ;
                        reply(MSG_BAD_NO_RETRY,logmessage) ;
                        free_all_var() ;
                        state = S_LOGGED ;
                        return 0 ;
                    } else if ( retcode > 0 ) {
                        syslog(BBFTPD_ERR,logmessage) ;
                        reply(MSG_BAD,logmessage) ;
                        free_all_var() ;
                        state = S_LOGGED ;
                        return 0 ;
                    } else {
                        if ( filesize != 0 ) {
                            /*
                            ** We are reserving the memory for the ports
                            */
                            if ( (myports = (int *) malloc (requestedstreamnumber*sizeof(int))) == NULL ) {
                                free_all_var() ;
                                syslog(BBFTPD_ERR,"Unable to allocate memory for myports") ;
                                sprintf(logmessage,"Unable to allocate memory for myports") ;
                                reply(MSG_BAD,logmessage) ;
                                state = S_LOGGED ;
                                return 0 ;
                            }
                            /*
                            ** We are reserving the memory for the children pid
                            */
                            if ( (mychildren = (int *) malloc (requestedstreamnumber*sizeof(int))) == NULL) {
                                free_all_var() ;
                                syslog(BBFTPD_ERR,"Unable to allocate memory for mychildren") ;
                                sprintf(logmessage,"Unable to allocate memory for mychildren") ;
                                reply(MSG_BAD,logmessage) ;
                                state = S_LOGGED ;
                                return 0 ;
                            }
                            /*
                            ** We are reserving the memory for the readbuffer
                            */
                            if ( (readbuffer = (char *) malloc (buffersizeperstream*1024)) == NULL) {
                                free_all_var() ;
                                syslog(BBFTPD_ERR,"Unable to allocate memory for readbuffer") ;
                                sprintf(logmessage,"Unable to allocate memory for readbuffer") ;
                                reply(MSG_BAD,logmessage) ;
                                state = S_LOGGED ;
                                return 0 ;
                            }
                            /*
                            ** We are reserving the memory for the compression buffer 
                            ** if needed
                            */
                            if ( (transferoption & TROPT_GZIP ) == TROPT_GZIP ) {
#ifdef WITH_GZIP
                                if ( (compbuffer = (char *) malloc (buffersizeperstream*1024)) == NULL) {
                                    free_all_var() ;
                                    syslog(BBFTPD_ERR,"Unable to allocate memory for compbuffer") ;
                                    sprintf(logmessage,"Unable to allocate memory for compbuffer") ;
                                    reply(MSG_BAD,logmessage) ;
                                    state = S_LOGGED ;
                                    return 0 ;
                                }
#else
                                transferoption = transferoption & ~TROPT_GZIP ;
#endif
                            }
                        }
                        if ( (receive_buffer = (char *) malloc (STORMESSLEN_V2)) == NULL ) {
                            syslog(BBFTPD_ERR,"Error allocating memory for MSG_TRANS_OK_V2 : %s",strerror(errno)) ;
                            free_all_var() ;
                            sprintf(logmessage,"Unable to allocate memory for message MSG_TRANS_OK_V2") ;
                            reply(MSG_BAD,logmessage) ;
                            state = S_LOGGED ;
                            return 0 ;
                          }
                        msg = (struct message *) receive_buffer ;
                        if ( protocolversion == 2) { /* ACTIVE MODE */
                          msg->code = MSG_TRANS_OK_V2 ;
                        } else { /* PASSIVE MODE */
                          msg->code = MSG_TRANS_OK_V3 ;
                        }
#ifndef WORDS_BIGENDIAN
                        msg->msglen = ntohl(STORMESSLEN_V2) ;
#else
                        msg->msglen = STORMESSLEN_V2 ;
#endif
                        if ( writemessage(outcontrolsock,receive_buffer,MINMESSLEN,sendcontrolto) < 0 ) {
                            syslog(BBFTPD_ERR,"Error writing MSG_TRANS_OK_V2 first part") ;
                            free_all_var() ;
                            FREE(receive_buffer) ;
                            return -1 ;
                        }
                        msg_store_v2 = (struct  mess_store_v2 *) receive_buffer ;
                        /*
                        ** Set up the transfer parameter
                        */
#ifndef WORDS_BIGENDIAN
                        msg_store_v2->transferoption       = transferoption ;
                        msg_store_v2->filemode             = ntohl(filemode) ;
                        strncpy(msg_store_v2->lastaccess,lastaccess,8) ;
                        msg_store_v2->lastaccess[8] = '0' ;
                        strncpy(msg_store_v2->lastmodif,lastmodif,8) ;
                        msg_store_v2->lastmodif[8] = '0' ;
                        msg_store_v2->sendwinsize          = ntohl(sendwinsize) ;
                        msg_store_v2->recvwinsize          = ntohl(recvwinsize) ;
                        msg_store_v2->buffersizeperstream  = ntohl(buffersizeperstream) ;
                        msg_store_v2->nbstream             = ntohl(requestedstreamnumber) ;
                        msg_store_v2->filesize             = ntohll(filesize) ;
#else
                        msg_store_v2->transferoption       = transferoption  ;       
                        msg_store_v2->filemode             =  filemode;
                        strncpy(msg_store_v2->lastaccess,lastaccess,8) ;
                        msg_store_v2->lastaccess[8] = '0' ;
                        strncpy(msg_store_v2->lastmodif,lastmodif,8) ;
                        msg_store_v2->lastmodif[8] = '0' ;
                        msg_store_v2->sendwinsize          = sendwinsize ;
                        msg_store_v2->recvwinsize          = recvwinsize ;
                        msg_store_v2->buffersizeperstream  = buffersizeperstream ;
                        msg_store_v2->nbstream             = requestedstreamnumber;
                        msg_store_v2->filesize             = filesize ;
#endif
                        if ( writemessage(outcontrolsock,receive_buffer,STORMESSLEN_V2,sendcontrolto) < 0 ) {
                            syslog(BBFTPD_ERR,"Error writing MSG_TRANS_OK_V2 second part") ;
                            bbftpd_storeunlink(realfilename) ;
                            free_all_var() ;
                            FREE(receive_buffer) ;
                            return -1 ;
                        }
                        if ( filesize == 0 ) {
	                    char statmessage[1024];
                            state = S_WAITING_CREATE_ZERO ;
                            sprintf(statmessage,"GET %s %s 0 0 0.0 0.0", currentusername, curfilename);
                            syslog(BBFTPD_NOTICE,statmessage);
                            free_all_var() ;
                        } else {
                            /* PASSIVE MODE: send ports */
                            if ( protocolversion >= 3 ) {
                              /*
                              ** We are reserving the memory for the ports
                              */
                              if ( (mysockets = (int *) malloc (requestedstreamnumber*sizeof(int))) == NULL ) {
                                syslog(BBFTPD_ERR,"Error allocating space for sockets") ;
                                bbftpd_storeunlink(realfilename) ;
                                free_all_var() ;
                                FREE(receive_buffer) ;
                                return -1 ;
                              }
                              if ( bbftpd_getdatasock(requestedstreamnumber) < 0 ) {
                                syslog(BBFTPD_ERR,"Error creating data sockets") ;
                                bbftpd_storeunlink(realfilename) ;
                                free_all_var() ;
                                FREE(receive_buffer) ;
                                return -1 ;
                              }
                              portfree = (int *)receive_buffer ;
                              porttosend = myports ;
                              for (i=0 ; i<requestedstreamnumber  ; i++) {
#ifndef WORDS_BIGENDIAN
                                *portfree++  = ntohl(ntohs(*porttosend++)) ;
#else
                                *portfree++ = *porttosend++ ;
#endif
                              }
/*          syslog(BBFTPD_INFO,"Port=%d,socket=%d\n",*myports,*mysockets) ;*/
                              if ( writemessage(outcontrolsock,receive_buffer,requestedstreamnumber*sizeof(int),sendcontrolto) < 0) {
                                syslog(BBFTPD_ERR,"Error writing MSG_TRANS_OK_V3 (ports)") ;
                                bbftpd_storeunlink(realfilename) ;
                                free_all_var() ;
                                FREE(receive_buffer) ;
                                return -1 ;
                              }
                            } /* END PASSIVE MODE */
                            state = S_WAITING_RETR_START ;
                        }
                        FREE(receive_buffer) ;
                        return 0 ;
                    }
                }
                default :{
                    syslog(BBFTPD_ERR,"Unkown message in S_WAITING_FILENAME state : %d",msgcode) ;
                    return -1 ;
                }
            }
        }
        
        /*
        ** Case S_WAITING_STORE_START
        **
        **         MSG_TRANS_START_V2
        **         MSG_TRANS_SIMUL
        **         MSG_ABORT
        */
        case S_WAITING_STORE_START : {
             switch (msgcode) {
                case MSG_TRANS_START_V2 :
                case MSG_TRANS_SIMUL :
                case MSG_TRANS_START_V3 :
                case MSG_TRANS_SIMUL_V3 :{
                    int simulation = ((msgcode == MSG_TRANS_SIMUL) || (msgcode == MSG_TRANS_SIMUL_V3)?1:0);
                    syslog(BBFTPD_DEBUG,"Receiving MSG_TRANS_START_V2") ;
                    if ( msglen != requestedstreamnumber*sizeof(int)) {
                        bbftpd_storeunlink(realfilename) ;
                        free_all_var() ;
                        reply(MSG_BAD,"Inconsistency between MSG_TRANS_START_V2 and message length") ;
                        syslog(BBFTPD_ERR,"Inconsistency between MSG_TRANS_START_V2 and message length") ;
                        return -1 ;
                    }
                    if (msgcode == MSG_TRANS_START_V2 || msgcode == MSG_TRANS_SIMUL) { /* ACTIVE MODE */
                      /*
                      ** Retreive port numbers
                      */
                      if ( readmessage(incontrolsock,(char *) myports,msglen,recvcontrolto) < 0 ) {
                        bbftpd_storeunlink(realfilename) ;
                        free_all_var() ;
                        syslog(BBFTPD_ERR,"Error reading MSG_TRANS_START_V2") ;
                        return -1 ;
                      } else {
#ifndef WORDS_BIGENDIAN
                        portfree = myports ;
                        for (i=0 ; i < requestedstreamnumber ; i++ ) {
                            *portfree = ntohl(*portfree) ;
                            portfree++ ;
                        }
#endif
                        portfree = myports ;
                        for (i=0 ; i < requestedstreamnumber ; i++ ) {
                            portfree++ ;
                        }
                      }
                    }
                    if ( (retcode = bbftpd_storetransferfile(realfilename,simulation,logmessage)) == 0 ) {
                        /*
                        ** Everything seems to work correctly
                        */
                        return 0 ;
                    }
                    if ( retcode > 0 ) {
                        /*
                        ** Something goes wrong
                        */
                        bbftpd_storeunlink(realfilename) ;
                        free_all_var() ;
                        state = S_LOGGED ;
                        return 0 ;
                    }
                    /*
                    ** 
                    */
                    bbftpd_storeunlink(realfilename) ;
                    free_all_var() ;
                    state = S_LOGGED ;
                    return -1 ;
                }
                case MSG_ABORT : {
                    syslog(BBFTPD_ERR,"Receive ABORT message") ;
                    bbftpd_storeunlink(realfilename) ;
                    free_all_var() ;
                    state = S_LOGGED ;
                    return 0 ;
                }
                default :{
                    syslog(BBFTPD_ERR,"Unkown message in S_WAITING_STORE_START state : %d",msgcode) ;
                    bbftpd_storeunlink(realfilename) ;
                    free_all_var() ;
                    return -1 ;
                }
            }
        }
        /*
        ** Case S_WAITING_CREATE_ZERO
        **
        **         MSG_CREATE_ZERO
        **         MSG_ABORT
        */
        case S_WAITING_CREATE_ZERO : {
             switch (msgcode) {
                case MSG_CREATE_ZERO : {
                    syslog(BBFTPD_ERR,"Receive MSG_CREATE_ZERO message ") ;
                    state = S_LOGGED ;
                    return 0 ;
                }
                case MSG_ABORT : {
                    syslog(BBFTPD_ERR,"Receive ABORT message") ;
                    state = S_LOGGED ;
                    return 0 ;
                }
                default :{
                    syslog(BBFTPD_ERR,"Unkown message in S_WAITING_CREATE_ZERO state : %d",msgcode) ;
                    return -1 ;
                }
            }
        }
        /*
        ** Case S_WAITING_RETR_START
        **
        **         MSG_TRANS_START_V2
        **         MSG_TRANS_SIMUL
        **         MSG_ABORT
        */
        case S_WAITING_RETR_START : {
             switch (msgcode) {
                case MSG_TRANS_START_V2 :
                case MSG_TRANS_SIMUL :
                case MSG_TRANS_START_V3 :
                case MSG_TRANS_SIMUL_V3 :{
                    int simulation = ((msgcode == MSG_TRANS_SIMUL) || (msgcode == MSG_TRANS_SIMUL_V3)?1:0);
                    syslog(BBFTPD_DEBUG,"Receiving MSG_TRANS_START_V2") ;
                    if ( msglen != requestedstreamnumber*sizeof(int)) {
                        free_all_var() ;
                        reply(MSG_BAD,"Inconsistency between MSG_TRANS_START_V2 and message length") ;
                        syslog(BBFTPD_ERR,"Inconsistency between MSG_TRANS_START_V2 and message length") ;
                        return -1 ;
                    }
                    if (msgcode == MSG_TRANS_START_V2 || msgcode == MSG_TRANS_SIMUL) { /* ACTIVE MODE */
                      /*
                      ** Retreive port numbers
                      */
                      if ( readmessage(incontrolsock,(char *) myports,msglen,recvcontrolto) < 0 ) {
                        free_all_var() ;
                        syslog(BBFTPD_ERR,"Error reading MSG_TRANS_START_V2") ;
                        return -1 ;
                      } else {
#ifndef WORDS_BIGENDIAN
                        portfree = myports ;
                        for (i=0 ; i < requestedstreamnumber ; i++ ) {
                            *portfree = ntohl(*portfree) ;
                            portfree++ ;
                        }
#endif
                        /*portfree = myports ;
                        for (i=0 ; i < requestedstreamnumber ; i++ ) {
                            portfree++ ;
							}*/
                      }
                    }
                    if ( (retcode = bbftpd_retrtransferfile(curfilename,simulation,logmessage)) == 0 ) {
                        /*
                        ** Everything seems to work correctly
                        */                       
                        return 0 ;
                    }
                    if ( retcode > 0 ) {
                        /*
                        ** Something goes wrong
                        */
                        syslog(BBFTPD_ERR,"retrtransferfile retcode > 0") ;
                        free_all_var() ;
                        state = S_LOGGED ;
                        return 0 ;
                    }
                    /*
                    ** 
                    */
                    syslog(BBFTPD_ERR,"retrtransferfile retcode < 0") ;
                    free_all_var() ;
                    state = S_LOGGED ;
                    return -1 ;
                }
                case MSG_ABORT : {
                    syslog(BBFTPD_ERR,"Receive ABORT message") ;
                    free_all_var() ;
                    state = S_LOGGED ;
                    return 0 ;
                }
                default :{
                    syslog(BBFTPD_ERR,"Unkown message in S_WAITING_RETR_START state : %d",msgcode) ;
                    return -1 ;
                }
            }
        }
        /*
        ** Case S_SENDING or S_RECEIVING
        */
        case S_SENDING : {
              switch (msgcode) {
                case MSG_ABORT :{
                    syslog(BBFTPD_ERR,"Receive MSG_ABORT while sending") ;
                    return -1 ;
                }
                default :{
                    syslog(BBFTPD_ERR,"Unkown message in S_SENDING state: %d",msgcode) ;
                    return -1 ;
                }    
            }
        }
        case S_RECEIVING : {
              switch (msgcode) {
                case MSG_ABORT :{
                    syslog(BBFTPD_ERR,"Receive MSG_ABORT while receiving") ;
                    bbftpd_storeclosecastfile(realfilename,logmessage) ;
                    bbftpd_storeunlink(realfilename) ;
                    return -1 ;
                }
                default :{
                    syslog(BBFTPD_ERR,"Unkown message in S_RECEIVING state : %d",msgcode) ;
                    bbftpd_storeclosecastfile(realfilename,logmessage) ;
                    bbftpd_storeunlink(realfilename) ;
                    return -1 ;
                }    
            }
        }
        /*
        ** Any other state
        */
        default : {
            syslog(BBFTPD_ERR,"Receive message in state %d",state) ;
            return -1 ;
        }
    }
}
       
        
