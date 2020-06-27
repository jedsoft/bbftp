/*
 * bbftpd/createadir.c
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
  
 createadir.c v 1.8.5  2000/04/28   - Creation of the routine.
              v 1.8.6  2000/04/04   - Add portage to OSF1
                                    - Avoid buffer overflow
              v 1.8.7  2000/05/24   - Correct portage to OSF1
              v 1.8.10 2000/08/11   - Portage to Linux
              v 1.9.0  2000/08/18   - Use configure to help portage
              v 1.9.4  2000/10/16   - Supress %m
                                    - Correct typo
              v 2.0.1  2001/04/23   - Correct indentation
              v 2.1.0  2001/05/30   - Correct syslog level
                                      
 *****************************************************************************/
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <bbftpd.h>
#include <common.h>
#include <daemon.h>
#include <structures.h>


extern int msgsock ;
extern	int	recvcontrolto ;

int createadir(int code, int msglen) {

    char    receive_buffer[MAXMESSLEN] ;
    int        savederrno ;
    char    logmessage[256] ;

#ifndef WORDS_BIGENDIAN
    msglen = ntohl(msglen) ;
#endif
    if ( msglen > MAXMESSLEN ) {
        /*
        ** In order to avoid buffer overflow we reject message to
        ** big
        */
        syslog(BBFTPD_ERR,"Message to big in createdir (%d,%d)",msglen,MAXMESSLEN) ;
        reply(MSG_BAD_NO_RETRY,"Directory too long") ;
        return -1 ;
    }
    /*
    ** Read the characteristics of the directory
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
    if ( code == MSG_MKDIR ) {
        syslog(BBFTPD_DEBUG,"Creating directory %s",receive_buffer) ;
    }
    /*
    ** We create the directory
    */
    if ( mkdir(receive_buffer,S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP) < 0 ) {
        /*
        ** Depending on errno we are going to tell the client to 
        ** retry or not
        */
        savederrno = errno ;
        sprintf(logmessage,"Error creation directory %s : %s",receive_buffer,strerror(errno)) ;
        syslog(BBFTPD_ERR,"Error creation directory %s : %s",receive_buffer,strerror(errno)) ;
        /*
        ** We tell the client not to retry in the following case (even in waiting
        ** WAITRETRYTIME the problem will not be solved) :
        **        EACCES        : Search permission denied
        **        EDQUOT        : No more quota 
        **        ENOSPC        : No more space
        **        ELOOP        : To many symbolic links on path
        **        ENAMETOOLONG: Path argument too long
        **        ENOTDIR        : A component in path is not a directory
        **        EROFS        : The path prefix resides on a read-only file system.
        **        ENOENT      : A component of the path prefix does not exist or is a null pathname.
        **        EEXIST      : The named file already exists.
        */
        if ( savederrno == EACCES ||
                savederrno == EDQUOT ||
                savederrno == ENOSPC ||
                savederrno == ELOOP ||
                savederrno == ENAMETOOLONG ||
                savederrno == ENOTDIR ||
                savederrno == EROFS ||
                savederrno == ENOENT ||
                savederrno == EEXIST ) {
            reply(MSG_BAD_NO_RETRY,logmessage) ;
        } else {
            reply(MSG_BAD,logmessage) ;
        }
        return 0 ;
    }
    reply(MSG_OK,"OK") ;
    return 0 ;
}
