/*
 * bbftpd/sendlist.c
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
        0  Keep the connection open (does not mean that the list has been
           successfully created)
        -1 Tell the calling program to close the connection
  
 sendlist.c v 1.8.8  2000/06/05 - Creation of the routine.
            v 1.8.9  2000/08/08 - Supress debug.
            v 1.8.10 2000/08/11 - Portage to Linux.
            v 1.9.0  2000/08/18 - Use configure to help portage
            v 1.9.4  2000/08/18 - Correct typo
            v 2.0.1  2001/04/23 - Correct indentation
            v 2.1.0  2001/05/30 - Correct syslog level
                          
 *****************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <netinet/in.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include <bbftpd.h>
#include <common.h>
#include <daemon.h>
#include <structures.h>
#ifdef DARWIN
#define GLOB_NOMATCH -3
#endif

#if HAVE_STRING_H
# include <string.h>
#endif

extern int msgsock ;
extern	int	recvcontrolto ;

int sendlist (int code, int msglen) {

    char    receive_buffer[MAXMESSLEN] ;
    char    send_buffer[MAXMESSLEN];
    char    logmessage[256] ;
    glob_t pg ;
    struct message *msg ;
    int        i ; 
    int        retcode ;
    char    *tmpfile ;

#ifndef WORDS_BIGENDIAN
    msglen = ntohl(msglen) ;
#endif
    if ( msglen > MAXMESSLEN ) {
        /*
        ** In order to avoid buffer overflow we reject message to
        ** big
        */
        syslog(BBFTPD_ERR,"Message to big in sendlist (%d,%d)",msglen,MAXMESSLEN) ;
        reply(MSG_BAD_NO_RETRY,"String too long") ;
        return -1 ;
    }
    /*
    ** Read the characteristics of the filelist to send
    */
    if ( readmessage(msgsock,receive_buffer,msglen,recvcontrolto) < 0 ) {
        /*
        ** Error ...
        */
        return -1 ;
    }
    /*
    ** receive_buffer contains the directory to create
    */
    receive_buffer[msglen] = '\0' ;
    if ( code == MSG_LIST ) {
        syslog(BBFTPD_DEBUG,"Getting file list %s",receive_buffer) ;
    }
    /*
    ** We get the filelist
    */
    if ( (retcode = glob(receive_buffer,0,0,&pg)) < 0 ) {
        if ( retcode == GLOB_NOMATCH ) {
            /*
            ** Just reply with a message MESS_LIST_REPL with msglen equal to zero
            */
            globfree(&pg) ;
            msg = (struct message *) send_buffer ;
            msg->code = MSG_LIST_REPL ;
            msg->msglen = 0 ;
            if ( writemessage(msgsock,send_buffer,MINMESSLEN,recvcontrolto) < 0 ) {
                syslog(BBFTPD_ERR,"Error sending LISTREPL part 1") ;
                return -1 ;
            }
            return 0 ;
        } else {
            globfree(&pg) ;
            sprintf(logmessage,"Error during glob call %d",retcode) ;
            reply(MSG_BAD,logmessage) ;
            return 0 ;
        }
    } else {
        /*
        ** The glob call was successfull
        */
        /*
        ** Calculate the total length of filename
        */
        msg = (struct message *) send_buffer ;
        msg->code = MSG_LIST_REPL ;
        msg->msglen = 0 ;
        for (i = 0 ; i < pg.gl_pathc ; i++ ) {
            tmpfile = pg.gl_pathv[i] ;
            msg->msglen = msg->msglen+strlen(tmpfile)+1 ;
        }
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(msg->msglen) ;
#endif
        if ( writemessage(msgsock,send_buffer,MINMESSLEN,recvcontrolto) < 0 ) {
            syslog(BBFTPD_ERR,"Error sending LISTREPL part 1") ;
            return -1 ;
        }
        /*
        ** Now send the file name separated by \0
        */
        for (i = 0 ; i < pg.gl_pathc ; i++ ) {
            tmpfile = pg.gl_pathv[i] ;
            if ( writemessage(msgsock,tmpfile,strlen(tmpfile)+1,recvcontrolto) < 0 ) {
                syslog(BBFTPD_ERR,"Error sending LISTREPL filename %s",tmpfile) ;
                return -1 ;
            }
        }
        return 0 ;
    }        
}
