/*
 * bbftpd/bbftpd_cd.c
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
        0  Keep the connection open (does not mean that the directory has been
           successfully created)
        -1 Tell the calling program to close the connection
  
 bbftpd_cd.c    v 2.0.0 2001/02/12  - Creation of the routine.
                v 2.0.1 2001/04/23  - Correct indentation
                v 2.1.0 2001/06/01  - Change routine name
                                     
 *****************************************************************************/
#include <bbftpd.h>

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <malloc.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <structures.h>

extern int transferoption ;
extern int recvcontrolto ;
int bbftpd_cd(int sock,int msglen) 
{

    char    *buffer ;
    int        savederrno ;
    char    *logmessage ;
    struct  mess_dir *msg_dir ;
    char    *dirname ;

    if ( (buffer = (char *) malloc (msglen+1) ) == NULL ) {
         syslog(BBFTPD_ERR,"Unable to malloc space for directory name (%d)",msglen) ;
         reply(MSG_BAD,"Unable to malloc space for directory name") ;
        return 0 ;
    }
    if ( (logmessage = (char *) malloc (msglen+1024) ) == NULL ) {
         syslog(BBFTPD_ERR,"Unable to malloc space for logmessage ") ;
         reply(MSG_BAD,"Unable to malloc space for logmessage") ;
        FREE(buffer) ;
        return 0 ;
    }
    /*
    ** Read the characteristics of the directory
    */
    if ( readmessage(sock,buffer,msglen,recvcontrolto) < 0 ) {
        /*
        ** Error ...
        */
        syslog(BBFTPD_ERR,"Error reading directory name") ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return -1 ;
    }
    /*
    ** buffer contains the directory to create
    */
    buffer[msglen] = '\0' ;
    msg_dir = (struct mess_dir *) buffer ;
     transferoption  = msg_dir->transferoption ;
    dirname = msg_dir->dirname ;
    
    if ( (transferoption & TROPT_RFIO) == TROPT_RFIO ) {
        syslog(BBFTPD_ERR,"Changing directory is not allowed under RFIO") ;
        sprintf(logmessage,"Changing directory is not allowed under RFIO") ;
        reply(MSG_BAD_NO_RETRY,logmessage) ;
        FREE(logmessage) ;
        FREE(buffer) ;
        return 0 ;
    }
    syslog(BBFTPD_DEBUG,"Changing directory to %s",dirname) ;
    /*
    ** We change the directory
    */
    if ( chdir(dirname) < 0 ) {
        /*
        ** Depending on errno we are going to tell the client to 
        ** retry or not
        */
        savederrno = errno ;
        sprintf(logmessage,"Error changing directory %s : %s ",dirname,strerror(errno)) ;
        syslog(BBFTPD_ERR,"Error changing directory %s : %s",dirname,strerror(errno)) ;
        /*
        ** We tell the client not to retry in the following case (even in waiting
        ** WAITRETRYTIME the problem will not be solved) :
        **        EACCES        : Search permission denied
        **        ELOOP        : To many symbolic links on path
        **        ENAMETOOLONG: Path argument too long
        **        ENOTDIR        : A component in path is not a directory
        **        ENOENT      : A component of the path prefix does not exist or is a null pathname.
        */
        if ( savederrno == EACCES ||
                savederrno == ELOOP ||
                savederrno == ENAMETOOLONG ||
                savederrno == ENOTDIR ||
                savederrno == ENOENT ) {
            reply(MSG_BAD_NO_RETRY,logmessage) ;
            FREE(buffer) ;
            FREE(logmessage) ;
        } else {
            reply(MSG_BAD,logmessage) ;
            FREE(logmessage) ;
            FREE(buffer) ;
        }
        return 0 ;
    } else {
        if ( getcwd(logmessage,msglen+1024)  == NULL ) {
            /*
            ** Unable to get current directory
            */
            sprintf(logmessage,"Unable to get current directory") ;
            reply(MSG_BAD,logmessage) ;
            FREE(logmessage) ;
            FREE(buffer) ;
            return 0 ;
        } 
    }
    reply(MSG_OK,logmessage) ;
    FREE(logmessage) ;
    FREE(buffer) ;
    return 0 ;
}
