/*
 * bbftpd/bbftpd_message.c
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

  
  
 bbftpd_message.c   v 0.0.0  1999/11/24
                    v 1.6.0  2000/03/24 - Take the readmessage routine from 
                                          bbftpc which is more flexible the
                                          only difference is that this one is
                                          logging into syslog
                    v 1.6.1  2000/03/24 - Portage to OSF1
                                        - Set the socket in non block mode
                    v 1.8.7  2000/05/30 - Portage to IRIX64
                    v 1.8.10 2000/08/11 - Portage to Linux
                    v 1.9.0  2000/08/24 - Use configure to help port
                    v 1.9.4  2000/10/16 - Supress %m
                    v 2.0.0  2000/12/07 - Introduce discardmessage routine
                                        - Replace recv by read for ssh mode
                                          (Tim Adye)
                    v 2.0.1 2001/04/23  - Correct indentation
                    v 2.1.0 2001/06/01  - Set all messages routine in one 
                                          file

*****************************************************************************/
#include <bbftpd.h>

#include <errno.h>
#include <stdlib.h>

#include <syslog.h>
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
#include <utime.h>
#if HAVE_STRING_H
# include <string.h>
#endif

#include <netinet/in.h>
#include <common.h>
#include <daemon.h>
#include <daemon_proto.h>
#include <structures.h>

extern int outcontrolsock ;
extern	int	sendcontrolto ;

int readmessage(int sock,char *buffer,int msglen,int to) 
{
    int    nbget ;
    int    retcode ;
    int        nfds ;
    fd_set    selectmask ; /* Select mask */
    struct timeval    wait_timer;

    nbget = 0 ;
/*
** start the reading loop
*/
    while ( nbget < msglen ) {
        nfds = sysconf(_SC_OPEN_MAX) ;
        FD_ZERO(&selectmask) ;
        FD_SET(sock,&selectmask) ;
        wait_timer.tv_sec  = to ;
        wait_timer.tv_usec = 0 ;
        if ( (retcode = select(nfds,&selectmask,0,0,&wait_timer) ) == -1 ) {
            /*
            ** Select error
            */
            syslog(BBFTPD_ERR,"Read message : Select error : MSG (%d,%d)",msglen,nbget) ;
            return -1 ;
        } else if ( retcode == 0 ) {
            syslog(BBFTPD_ERR,"Read message : Time out : MSG (%d,%d)",msglen,nbget) ;
            return -1 ;
        } else {
            retcode = read(sock,&buffer[nbget],msglen-nbget) ;
            if ( retcode < 0 ) {
                syslog(BBFTPD_ERR,"Read message : Receive error : MSG (%d,%d) : %s",msglen,nbget,strerror(errno)) ;
                return -1 ;
            } else if ( retcode == 0 ) {
                syslog(BBFTPD_ERR,"Read message : Connexion breaks") ;
                return -1 ;
            } else {
                nbget = nbget + retcode ;
            }
        }
    }
    return(0) ;
}

int discardmessage(int sock,int msglen,int to) 
{
    int    nbget ;
    int    retcode ;
    int        nfds ;
    fd_set    selectmask ; /* Select mask */
    struct timeval    wait_timer;
    char buffer[256] ;
    
    /*
    ** We are going to read buflen by buflen till the message 
    ** is exhausted
    */
    
    nbget = 0 ;
    while ( nbget < msglen ) {
        nfds = sysconf(_SC_OPEN_MAX) ;
        FD_ZERO(&selectmask) ;
        FD_SET(sock,&selectmask) ;
        wait_timer.tv_sec  = to ;
        wait_timer.tv_usec = 0 ;
        if ( (retcode = select(nfds,&selectmask,0,0,&wait_timer) ) == -1 ) {
            /*
            ** Select error
            */
            syslog(BBFTPD_ERR,"Discard message : Select error : MSG (%d,%d)",msglen,nbget) ;
            return -1 ;
        } else if ( retcode == 0 ) {
            syslog(BBFTPD_ERR,"Discard message : Time out : MSG (%d,%d)",msglen,nbget) ;
            return -1 ;
        } else {
            retcode = recv(sock,buffer,sizeof(buffer),0) ;
            if ( retcode < 0 ) {
                syslog(BBFTPD_ERR,"Discard message : Receive error : MSG (%d,%d) : %s",msglen,nbget,strerror(errno)) ;
                return -1 ;
            } else if ( retcode == 0 ) {
                syslog(BBFTPD_ERR,"Discard message : Connexion breaks") ;
                return -1 ;
            } else {
                nbget = nbget + retcode ;
            }
        }
    }
    return(0) ;
}
        
int writemessage(int sock,char *buffer,int msglen,int to) 
{
    int    nbsent ;
    int    retcode ;
    int        nfds ;
    fd_set    selectmask ; /* Select mask */
    struct timeval    wait_timer;

    nbsent = 0 ;
/*
** start the writing loop
*/
    while ( nbsent < msglen ) {
        nfds = sysconf(_SC_OPEN_MAX) ;
        FD_ZERO(&selectmask) ;
        FD_SET(sock,&selectmask) ;
        wait_timer.tv_sec  = to ;
        wait_timer.tv_usec = 0 ;
        if ( (retcode = select(nfds,0,&selectmask,0,&wait_timer) ) == -1 ) {
            /*
            ** Select error
            */
            syslog(BBFTPD_ERR,"Write message : Select error : MSG (%d,%d)",msglen,nbsent) ;
            return -1 ;
        } else if ( retcode == 0 ) {
            syslog(BBFTPD_ERR,"Write message : Time out : MSG (%d,%d)",msglen,nbsent) ;
            return -1 ;
        } else {
            retcode = write(sock,&buffer[nbsent],msglen-nbsent) ;
            if ( retcode < 0 ) {
                syslog(BBFTPD_ERR,"Write message : write error : MSG (%d,%d) : %s",msglen,nbsent,strerror(errno)) ;
                return -1 ;
            } else if ( retcode == 0 ) {
                syslog(BBFTPD_ERR,"Write message : Connexion breaks") ;
                return -1 ;
            } else {
                nbsent = nbsent + retcode ;
            }
        }
    }
    return(0) ;
}

void reply(int n, char *str)
{
    struct message    *mess ;
    int    lentosend ;
    char buf[MINMESSLEN] ;
    
    mess = (struct message *) buf ;
    mess->code = n ;
#ifndef WORDS_BIGENDIAN
    mess->msglen = ntohl(strlen(str)) ;
#else
    mess->msglen = strlen(str) ;
#endif
    if ( writemessage(outcontrolsock,buf,MINMESSLEN,sendcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error on reply") ;
        clean_child() ;
        exit(1) ;
    }
    lentosend = strlen(str) ;
    if ( writemessage(outcontrolsock,str,lentosend,sendcontrolto) < 0 ) {
        syslog(BBFTPD_ERR,"Error on reply") ;
        clean_child() ;
        exit(1) ;
    }
}
 
