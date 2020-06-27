/*
 * bbftpd/bbftpd_list.c
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
  
 bbftpd_list.c  v 2.0.0  2001/02/27 - Creation of the routine.
                v 2.0.1  2001/04/23 - Correct indentation
                v 2.0.2  2001/05/04 - Change the philosophy in order
                                      to bypass CASTOR rfio_rewinddir bug 
                v 2.1.0  2001/06/01 - Reorganise routines as in bbftp_

 *****************************************************************************/
#include <dirent.h>
#include <errno.h>
#include <fnmatch.h>
#include <netinet/in.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>

#include <bbftpd.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <status.h>
#include <structures.h>

extern int  transferoption ;
extern int  outcontrolsock ;
extern	int	recvcontrolto ;
extern	int	sendcontrolto ;

int bbftpd_list(char *pattern,char *logmessage) 
{

    char    *filelist ;
    int     filelistlen ;
    int     retcode ;
    char    send_buffer[MINMESSLEN] ;
    struct message *msg ;
    
    
    if ( (retcode = bbftpd_retrlistdir(pattern,&filelist,&filelistlen,logmessage) ) < 0) {
        reply(MSG_BAD_NO_RETRY,logmessage) ;
        return 0 ;
    } else if ( retcode > 0 ) {
        reply(MSG_BAD,logmessage) ;
        return 0 ;
    } else {
        msg = (struct message *) send_buffer ;
        msg->code = MSG_LIST_REPL_V2 ;
#ifndef WORDS_BIGENDIAN
        msg->msglen = ntohl(filelistlen) ;
#else 
        msg->msglen = filelistlen ;
#endif
        if ( writemessage(outcontrolsock,send_buffer,MINMESSLEN,recvcontrolto) < 0 ) {
            syslog(BBFTPD_ERR,"Error sending LISTREPL_V2 part 1") ;
            FREE(filelist) ;
            return -1 ;
        }
        if (filelistlen != 0 ) {
            if ( writemessage(outcontrolsock,filelist,filelistlen,recvcontrolto) < 0 ) {
                FREE(filelist) ;
                syslog(BBFTPD_ERR,"Error sending filelist") ;
                return -1 ;
            }
            FREE(filelist) ;
            return 0 ;
        }
        return 0 ;
    }
}
