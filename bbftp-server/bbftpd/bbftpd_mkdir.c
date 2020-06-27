/*
 * bbftpd/bbftpd_mkdir.c
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
  
 bbftpd_mkdir.c v 2.0.0  2001/02/12 - Creation of the routine.
                v 2.0.1  2001/04/23 - Correct indentation
                                    - Port to IRIX
                v 2.1.0  2001/06/01 - Reorganise routines as in bbftp_
                                      
 *****************************************************************************/


#include <stdio.h>
#include <syslog.h>
#include <utime.h>
#include <stdlib.h>

#include <bbftpd.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <netinet/in.h>
#include <structures.h>

extern int transferoption ;
extern	int	recvcontrolto ;

int bbftpd_mkdir(int sock,int msglen) 
{
    char    *buffer ;
    char    *logmessage ;
    struct  mess_dir *msg_dir ;
    char    *dirname ;
    int     recursif ;
    int     retcode ;

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
    buffer[msglen] = '\0' ;
    msg_dir = (struct mess_dir *) buffer ;
    transferoption  = msg_dir->transferoption ;
    dirname = msg_dir->dirname ;
    if ( (transferoption & TROPT_DIR ) == TROPT_DIR ) {
        recursif = 1 ;
    } else {
        recursif = 0 ;
    }
    if ( (retcode = bbftpd_storemkdir(dirname,logmessage,recursif)) < 0 ) {
        reply(MSG_BAD_NO_RETRY,logmessage) ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return 0 ;
    } else if (retcode > 0 ) {
        reply(MSG_BAD,logmessage) ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return 0 ;
    } else {
        sprintf(logmessage,"Directory %s created",dirname) ;
        reply(MSG_OK,logmessage) ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return 0 ;
    }
}
