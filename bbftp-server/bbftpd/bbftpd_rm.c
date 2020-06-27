/*
 * bbftpd/bbftpd_rm.c
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
        0  Keep the connection open (does not mean that the file has been
           successfully created)
        -1 Tell the calling program to close the connection
  
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

int bbftpd_rm(int sock,int msglen) 
{
    char    *buffer ;
    char    *logmessage ;
    struct  mess_dir *msg_file ;
    char    *filename ;
    int     recursif ;
    int     retcode ;

    if ( (buffer = (char *) malloc (msglen+1) ) == NULL ) {
        syslog(BBFTPD_ERR,"Unable to malloc space for file name (%d)",msglen) ;
        reply(MSG_BAD,"Unable to malloc space for file name") ;
        return 0 ;
    }
    if ( (logmessage = (char *) malloc (msglen+1024) ) == NULL ) {
        syslog(BBFTPD_ERR,"Unable to malloc space for logmessage ") ;
        reply(MSG_BAD,"Unable to malloc space for logmessage") ;
        FREE(buffer) ;
        return 0 ;
    }
    /*
    ** Read the characteristics of the file
    */
    if ( readmessage(sock,buffer,msglen,recvcontrolto) < 0 ) {
        /*
        ** Error ...
        */
        syslog(BBFTPD_ERR,"Error reading file name") ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return -1 ;
    }
    buffer[msglen] = '\0' ;
    msg_file = (struct mess_dir *) buffer ;
    transferoption  = msg_file->transferoption ;
    filename = msg_file->dirname ;
    /*
	if ( (transferoption & TROPT_DIR ) == TROPT_DIR ) {
        recursif = 1 ;
    } else {
        recursif = 0 ;
    }*/
    if ( (retcode = bbftpd_storeunlink(filename)) < 0 ) {
        sprintf(logmessage,"Unable to delete %s",filename) ;
        reply(MSG_BAD_NO_RETRY,logmessage) ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return 0 ;
    } else {
        sprintf(logmessage,"%s has been deleted",filename) ;
        reply(MSG_OK,logmessage) ;
        FREE(buffer) ;
        FREE(logmessage) ;
        return 0 ;
    }
}
